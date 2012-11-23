/*
 * mysqlcb.c
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

#include "mysqlcb.h"
#include "mysqlcb_buffer.h"

mycb_conf_t mycb_conf;

void mycb_init() {
	strncpy(mycb_conf.host, "127.0.0.1", sizeof(mycb_conf.host));
	strncpy(mycb_conf.ralayLogPath, "/tmp/ralay", sizeof(mycb_conf.ralayLogPath));
	strncpy(mycb_conf.userName, "root", sizeof(mycb_conf.userName));
	mycb_conf.port = 3306;
	mycb_conf.serverId = 12345;
	mycb_buf_init(&mycb_conf.readBuf);
	mycb_buf_init(&mycb_conf.writeBuf);
	mycb_conf.sock = 0;
	mycb_conf.ralayLogFd = NULL;
	mycb_conf.isRun = 1;
}
void mycb_destrory() {
	if (mycb_conf.sock != 0) {
		close(mycb_conf.sock);
	}
	mycb_conf.sock = 0;
	if (mycb_conf.ralayLogFd != NULL ) {
		fclose(mycb_conf.ralayLogFd);
	}
	mycb_conf.ralayLogFd = NULL;
}

int mycb_start() {
	int ret = 0;

	//连接master
	if ((ret = mycb_connect()) != 0) {
		fprintf(stdout, "[mysqlcb] connect master ('%s:%s@%s:%d') fail, socket id is '%d'  \n", mycb_conf.userName,
				mycb_conf.password, mycb_conf.host, mycb_conf.port, mycb_conf.sock);
		return -1;
	}
	if (mycb_conf.verbose > 0) {
		fprintf(stdout, "[mysqlcb] connect master ('%s@%s:%d') , socket id is '%d'  \n", mycb_conf.userName,
				mycb_conf.host, mycb_conf.port, mycb_conf.sock);
	}

	//初始化ralaylog
	if ((ret = mycb_init_ralay_log()) != 0) {
		fprintf(stderr, "[mysqlcb] [error] init ralay log fail >\n");
		return -1;
	}
	if (mycb_conf.verbose > 0) {
		fprintf(stdout, "[mysqlcb] [info] start file '%s', start pos '%d' \n", mycb_conf.ralayLogName, mycb_conf.ralayLogPos);
	}

	//发送dump命令
	if ((ret = mycb_send_dump_cmd()) != 0) {
		fprintf(stderr, "[mysqlcb] [error] send dump command fail\n");
		return -1;
	}
	if (mycb_conf.verbose > 0) {
		fprintf(stderr, "[mysqlcb] [info] send dump command\n");
	}

	//循环读取event
	if (mycb_conf.verbose > 0) {
		fprintf(stderr, "[mysqlcb] [info] start loop event\n");
	}
	ret = mycb_loop_event();
	if (mycb_conf.verbose > 0) {
		fprintf(stderr, "[mysqlcb] [info] end loop event\n");
	}

	return ret;
}

int mycb_connect() {
	mycb_conf.mysql = mysql_init(NULL );
	mycb_conf.mysql = mysql_real_connect(mycb_conf.mysql, mycb_conf.host, mycb_conf.userName, mycb_conf.password, "",
			mycb_conf.port, NULL, 0);
	if (mycb_conf.mysql == NULL ) {
		return -1;
	}
	mycb_conf.sock = mycb_conf.mysql->net.fd;

	//set none block
	int flag = fcntl(mycb_conf.sock, F_GETFL, 0);
	if (fcntl(mycb_conf.sock, F_SETFL, flag | O_NONBLOCK) != 0) {
		return -1;
	}
	return 0;
}

int mycb_init_ralay_log() {
	DIR * dp = NULL;
	struct dirent *ent = NULL;

	if ((dp = opendir(mycb_conf.ralayLogPath)) == NULL ) {
		fprintf(stderr, "[mysqlcb] [error] open ralay log path '%s' fail\n", mycb_conf.ralayLogPath);
		return -1;
	}

	//先從logpath里读取
	char maxFile[256] = { '\0' };
	int maxNum = 0;
	char suffix[256] = { '\0' };
	char tmpLogName[256] = { '\0' };
	while ((ent = readdir(dp)) != NULL ) {
		strncpy(tmpLogName, ent->d_name, sizeof(tmpLogName));
		char * pos = rindex(tmpLogName, '.');
		if (pos == NULL ) {
			continue;
		}
		pos++;
		strncpy(suffix, pos, sizeof(suffix));
		int num = atoi(suffix);
		if (num > maxNum) {
			maxNum = num;
			strncpy(maxFile, tmpLogName, sizeof(maxFile));
		}
	}
	closedir(dp);

	int ret;
	if (maxNum == 0) {
		if ((ret = mycb_load_max_ralay_from_db(maxFile, sizeof(maxFile))) != 0) {
			return -1;
		}
	}
	strncpy(mycb_conf.ralayLogName, maxFile, sizeof(mycb_conf.ralayLogName));

	//计算log ralay start pos
	snprintf(mycb_conf.ralayLogFullName, sizeof(mycb_conf.ralayLogFullName), "%s/%s", mycb_conf.ralayLogPath,
			mycb_conf.ralayLogName);

	if ((ret = mycb_open_ralay_log()) != 0) {
		fprintf(stderr, "[mysqlcb] [error] open ralay log file fail '%s' fail\n", mycb_conf.ralayLogFullName, __FILE__, __func__,
				__LINE__);
		return -1;
	}

	mycb_conf.ralayLogPos = ftell(mycb_conf.ralayLogFd);
	if (mycb_conf.ralayLogPos == 0) {
		//write file head
		if ((ret = mycb_write_file_head()) != 0) {
			return -1;
		}
		mycb_conf.ralayLogPos = ftell(mycb_conf.ralayLogFd);
	}

	return 0;
}
int mycb_load_max_ralay_from_db(char * maxFile, uint len) {
	//沒有任何log,则从数据库取最新的binlog文件，并从文件起始处同步
	MYSQL_RES *result = NULL;
	uint ret;
	int maxNum = 0;
	char suffix[256] = { '\0' };
	char tmpLogName[256] = { '\0' };

	//因为是从起始处同步，为了避免文件过大，浪费同步时间，强制请求master产生新的binlog
	if ((ret = mysql_query(mycb_conf.mysql, "flush logs")) != 0) {
		fprintf(stderr, "[mysqlcb] [error] flush logs fail:%s\n", mysql_error(mycb_conf.mysql));
		return -1;
	}

	if ((ret = mysql_query(mycb_conf.mysql, "show master logs")) != 0) {
		fprintf(stderr, "[mysqlcb] [error] show master logs fail:%s\n", mysql_error(mycb_conf.mysql));
		return -1;
	}

	if ((result = mysql_store_result(mycb_conf.mysql)) == NULL ) {
		fprintf(stderr, "[mysqlcb] [error] show master logs fail:%s\n", mysql_error(mycb_conf.mysql));
		return -1;
	}
	MYSQL_ROW row = NULL;
	while ((row = mysql_fetch_row(result)) != NULL ) {
		strncpy(tmpLogName, row[0], sizeof(tmpLogName));

		char * pos = rindex(tmpLogName, '.');
		if (pos == NULL ) {
			continue;
		}
		pos++;
		strncpy(suffix, pos, sizeof(suffix));
		int num = atoi(suffix);
		if (num > maxNum) {
			maxNum = num;
			strncpy(maxFile, tmpLogName, len);
		}
	}
	if (maxNum == 0) {
		return -1;
	}
	return 0;
}
int mycb_send_dump_cmd() {
	mycb_buf_write_uint8(&mycb_conf.writeBuf, 0x12); //cmd
	mycb_buf_write_uint32(&mycb_conf.writeBuf, mycb_conf.ralayLogPos);
	mycb_buf_write_uint16(&mycb_conf.writeBuf, 0);
	mycb_buf_write_uint32(&mycb_conf.writeBuf, mycb_conf.serverId);
	mycb_buf_write_null_string(&mycb_conf.writeBuf, mycb_conf.ralayLogName);

	int ret;
	if ((ret = mycb_write_packet(0)) != 0) {
		return -1;
	}

	if ((ret = mycb_read_packet()) != 0) {
		return -1;
	}

	uint type = mycb_buf_get_uint8(&mycb_conf.readBuf);
	if (type != 0x00) {
		mycb_print_error_packet();
		return -1;
	}

	return 0;
}
void mycb_print_error_packet() {
	mycb_buf_inc_cur(&mycb_conf.readBuf, 3);
	uint remain = mycb_conf.readBuf.limit - mycb_conf.readBuf.cur;
	char msg[remain + 1];

	bzero(msg, sizeof(msg));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, msg, remain);
	fprintf(stderr, "[mysqlcb] [error] error packet result '%s' \n", msg);
}
int mycb_loop_event() {
	int ret;
	while (1) {
		if (!mycb_conf.isRun) {
			break;
		}
		if (!mycb_can_read()) {
			continue;
		}
		if ((ret = mycb_read_packet()) == 0) {
			mycb_parse_event();
		}
	}
	return 0;
}

int mycb_open_ralay_log() {
	if (mycb_conf.ralayLogFd != NULL ) {
		return 0;
	}
	if ((mycb_conf.ralayLogFd = fopen(mycb_conf.ralayLogFullName, "a+")) == NULL ) {
		return -1;
	}
	uint ret;
	if ((ret = fseek(mycb_conf.ralayLogFd, 0, SEEK_END)) != 0) {
		return -1;
	}
	return 0;
}
int mycb_switch_ralay_log() {
	if (mycb_conf.ralayLogFd != NULL ) {
		fclose(mycb_conf.ralayLogFd);
	}
	mycb_conf.ralayLogFd = NULL;
	int ret;
	if ((ret = mycb_open_ralay_log()) != 0) {
		return -1;
	}
	return 0;
}

int mycb_write_packet(uint sid) {
	char header[4];
	char * ptr = header;
	uint bodyLen = mycb_buf_get_len(&mycb_conf.writeBuf);
	uchar tmp;
	tmp = (uchar) bodyLen & 0xff;
	memcpy(ptr++, &tmp, 1);
	tmp = (uchar) (bodyLen >> 8) & 0xff;
	memcpy(ptr++, &tmp, 1);
	tmp = (uchar) (bodyLen >> 16) & 0xff;
	memcpy(ptr++, &tmp, 1);

	tmp = (uchar) sid;
	memcpy(ptr++, &tmp, 1);

	if (mycb_write(header, sizeof(header)) != 0) {
		return -1;
	}
	if (mycb_write(mycb_conf.writeBuf.start, bodyLen) != 0) {
		return -1;
	}
	mycb_buf_reset(&mycb_conf.writeBuf);
	return 0;
}

int mycb_read_packet() {
	char header[4];

	mycb_buf_reset(&mycb_conf.readBuf);
	if (mycb_read((void *) header, 4) != 0) {
		return -1;
	}
	mycb_buf_set_buf(&mycb_conf.readBuf, header, sizeof(header));
	uint bodyLen = mycb_buf_read_uint24(&mycb_conf.readBuf);
	uint sid = mycb_buf_read_uint8(&mycb_conf.readBuf);

	char body[bodyLen];
	if (mycb_read((void *) body, bodyLen) != 0) {
		return -1;
	}
	mycb_buf_set_buf(&mycb_conf.readBuf, body, sizeof(body));
	return 0;
}
int mycb_write_to_ralay_file() {
	if (mycb_conf.verbose > 0) {
		fprintf(stdout, "[mysqlcb] [ralay] write to file size:%ld\n", mycb_conf.readBuf.limit - mycb_conf.readBuf.start - 1);
	}
	fwrite(&mycb_conf.readBuf.start + 1, mycb_conf.readBuf.limit - mycb_conf.readBuf.start - 1, 1,
			mycb_conf.ralayLogFd);
	if (ferror(mycb_conf.ralayLogFd) > 0) {
		return -1;
	}
	fflush(mycb_conf.ralayLogFd);
	return 0;
}

int mycb_parse_event() {
	//header
	mycb_buf_read_uint8(&mycb_conf.readBuf);
	uint time = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint type = mycb_buf_read_uint8(&mycb_conf.readBuf);
	uint serverId = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint size = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint pos = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint flags = mycb_buf_read_uint16(&mycb_conf.readBuf);

	mycb_conf.eventTime = time;

	if (mycb_conf.verbose > 0) {
		fprintf(stdout, "[mysqlcb] [event] time=%d,type=%d,serverId=%d,size=%d,pos=%d,flags=%d\n", time, type, serverId,
				size, pos, flags);
	}
	if (pos != 0) {
		if (mycb_write_to_ralay_file() != 0) {
			return -1;
		}
	}

	if (type == 0x0f) {
		mycb_parse_desc_event();
	} else if (type == 0x02) {
		mycb_parse_query_event();
	} else if (type == 0x04) {
		mycb_parse_rotate_event();
	} else if (type == 0x13) {
		mycb_parse_tablemap_event();
	} else if (type == 0x14) {
		mycb_parse_rows_event(ROW_EVENT_ADD, 0);
	} else if (type == 0x15) {
		mycb_parse_rows_event(ROW_EVENT_UPDATE, 0);
	} else if (type == 0x16) {
		mycb_parse_rows_event(ROW_EVENT_DELETE, 1);
	} else if (type == 0x17) {
		mycb_parse_rows_event(ROW_EVENT_ADD, 0);
	} else if (type == 0x18) {
		mycb_parse_rows_event(ROW_EVENT_UPDATE, 1);
	} else if (type == 0x19) {
		mycb_parse_rows_event(ROW_EVENT_DELETE, 1);
	} else if (type == 0x1e) {
		mycb_parse_rows_event(ROW_EVENT_ADD, 2);
	} else if (type == 0x1f) {
		mycb_parse_rows_event(ROW_EVENT_UPDATE, 2);
	} else if (type == 0x20) {
		mycb_parse_rows_event(ROW_EVENT_DELETE, 2);
	}

	return 0;
}
int mycb_parse_desc_event() {
	uint ver = mycb_buf_read_uint16(&mycb_conf.readBuf);
	char version[51];
	bzero(version, sizeof(version));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, version, 50);

	uint time = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint headerLen = mycb_buf_read_uint8(&mycb_conf.readBuf);

	char headerLenMap[headerLen + 1];
	bzero(headerLenMap, sizeof(headerLenMap));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, headerLenMap, headerLen);

	if (ver != 4) {
		//不支持mysql5.0以下的版本
		return -1;
	}

	return 0;

}
int mycb_parse_query_event() {
	uint slaveProxyId = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint execTime = mycb_buf_read_uint32(&mycb_conf.readBuf);
	uint schemaLen = mycb_buf_read_uint8(&mycb_conf.readBuf);
	uint errCode = mycb_buf_read_uint16(&mycb_conf.readBuf);
	uint statusVarsLen = mycb_buf_read_uint16(&mycb_conf.readBuf);

	char statusVars[statusVarsLen + 1];
	bzero(statusVars, sizeof(statusVars));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, statusVars, statusVarsLen);

	char schema[schemaLen + 1];
	bzero(schema, sizeof(schema));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, schema, schemaLen);

	mycb_buf_read_uint8(&mycb_conf.readBuf);

	uint remain = mycb_conf.readBuf.limit - mycb_conf.readBuf.cur;
	char sql[remain + 1];
	bzero(sql, sizeof(sql));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, sql, remain);

	event_t event;
	event.db = schema;
	event.table = NULL;
	event.time = mycb_conf.eventTime;
	event.sql = sql;
	event.row = NULL;
	event.type = EVENT_TYPE_SQL_EXEC;
	mycb_conf.eventHandler(&event);

	return 0;
}
int mycb_parse_tablemap_event() {

	unsigned long long tableId = mycb_buf_read_uint48(&mycb_conf.readBuf);
	uint flag = mycb_buf_read_uint16(&mycb_conf.readBuf);

	uint schemaLen = mycb_buf_read_uint8(&mycb_conf.readBuf);
	char schema[schemaLen + 1];
	bzero(schema, sizeof(schema));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, schema, schemaLen);
	mycb_buf_read_uint8(&mycb_conf.readBuf);
	snprintf(mycb_conf.db, 256, "%s", schema);

	uint tableLen = mycb_buf_read_uint8(&mycb_conf.readBuf);
	char table[tableLen + 1];
	bzero(table, sizeof(table));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, table, tableLen);
	mycb_buf_read_uint8(&mycb_conf.readBuf);
	snprintf(mycb_conf.table, 256, "%s", table);

	unsigned long long colCount = mycb_buf_read_vint(&mycb_conf.readBuf);
	char colTypeDef[colCount + 1];
	bzero(colTypeDef, sizeof(colTypeDef));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, colTypeDef, colCount);

	//保存表中字段类型,供后续读取用
	uint i;
	for (i = 0; i < colCount; i++) {
		mycb_conf.fieldTypeMap[i] = colTypeDef[i];
	}

	unsigned long long colTypeLen = mycb_buf_read_vint(&mycb_conf.readBuf);
	uint size;
	uint meta;
	uint sizeByte;
	uint byte0;
	uint byte1;
	uint scale;
	uint precision;
	uint nbits;

	for (i = 0; i < colCount; i++) {
		switch (mycb_conf.fieldTypeMap[i]) {
		case MYSQLCB_TYPE_BIT:
			meta = mycb_buf_read_uint16(&mycb_conf.readBuf);
			nbits = ((meta >> 8) * 8) + (meta & 0xff);
			mycb_conf.fieldLenMap[i] = (nbits + 7) / 8;
			break;
		case MYSQLCB_TYPE_STRING:
			byte0 = mycb_buf_read_uint8(&mycb_conf.readBuf);
			byte1 = mycb_buf_read_uint8(&mycb_conf.readBuf);
			if ((byte0 & 0x30) != 0x30) {
				//long char
				size = byte1 | (((byte0 & 0x30) ^ 0x30) << 4);
				mycb_conf.fieldTypeMap[i] = byte0 | 0x30;
				sizeByte = 1;
				while ((size = size >> 8) > 0) {
					sizeByte++;
				}
				mycb_conf.fieldLenMap[i] = sizeByte;
				break;
			} else if (byte0 == MYSQLCB_TYPE_ENUM || byte0 == MYSQLCB_TYPE_SET) {
				mycb_conf.fieldTypeMap[i] = byte0;
				mycb_conf.fieldLenMap[i] = byte1;
			}
			break;

		case MYSQLCB_TYPE_VAR_STRING:
		case MYSQLCB_TYPE_VARCHAR:
			size = mycb_buf_read_uint16(&mycb_conf.readBuf);
			sizeByte = 1;
			while ((size = size >> 8) > 0) {
				sizeByte++;
			}
			mycb_conf.fieldLenMap[i] = sizeByte;
			break;
		case MYSQLCB_TYPE_NEWDECIMAL:
			precision = mycb_buf_read_uint8(&mycb_conf.readBuf);
			scale = mycb_buf_read_uint8(&mycb_conf.readBuf);
			mycb_conf.fieldLenMap[i] = mycb_decimal_bin_size(precision, scale);
			break;
		case MYSQLCB_TYPE_BLOB:
			mycb_conf.fieldLenMap[i] = mycb_buf_read_uint8(&mycb_conf.readBuf);
			break;
		case MYSQLCB_TYPE_DOUBLE:
		case MYSQLCB_TYPE_FLOAT:
			size = mycb_buf_read_uint8(&mycb_conf.readBuf);
			sizeByte = 1;
			while ((size = size >> 8) > 0) {
				sizeByte++;
			}
			mycb_conf.fieldLenMap[i] = sizeByte;
			break;
		default:
			mycb_conf.fieldLenMap[i] = 0;
			break;
		}
	}

	uint nullBitmapLen = (colCount + 8) / 7;
	char nullBitmap[nullBitmapLen + 1];
	bzero(nullBitmap, sizeof(nullBitmap));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, nullBitmap, nullBitmapLen);

	return 0;
}
int mycb_parse_rows_event(uint type, uint version) {
	unsigned long long tableId = mycb_buf_read_uint48(&mycb_conf.readBuf);
	uint flag = mycb_buf_read_uint16(&mycb_conf.readBuf);
	uint i;

	//body
	unsigned long long colCount = mycb_buf_read_vint(&mycb_conf.readBuf);

	uint presentBitmapLen = (colCount + 7) / 8;
	char presentBitmap[presentBitmapLen + 1];
	bzero(presentBitmap, sizeof(presentBitmap));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, presentBitmap, presentBitmapLen);

	uint presentBitmapLen2 = (colCount + 7) / 8;
	char presentBitmap2[presentBitmapLen2 + 1];

	//UPDATE_ROWS_EVENTv1 or v2
	if (type == ROW_EVENT_UPDATE && (version == 1 || version == 2)) {
		bzero(presentBitmap2, sizeof(presentBitmapLen2));
		mycb_buf_read_fixed_string(&mycb_conf.readBuf, presentBitmap2, presentBitmapLen2);
	}

	//初始化event
	event_t event;
	event.time = mycb_conf.eventTime;
	event.db = mycb_conf.db;
	event.table = mycb_conf.table;
	event.len = 0;

	//读取行
	char *curPresentBitmap = presentBitmap;
	while (mycb_conf.readBuf.limit > mycb_conf.readBuf.cur) {
		//读取null标志
		uint nullBitmapLen = (colCount + 7) / 8;
		char nullBitmap[nullBitmapLen + 1];
		bzero(nullBitmap, sizeof(nullBitmap));
		mycb_buf_read_fixed_string(&mycb_conf.readBuf, nullBitmap, nullBitmapLen);

		//初始化row
		row_t row;
		row.fieldCount = colCount;
		row.fields = (field_t *) calloc(row.fieldCount, sizeof(field_t));

		uint fieldNum = 0;
		for (fieldNum = 0; fieldNum < colCount; fieldNum++) {
			//初始化field
			row.fields[fieldNum].isUse = 1;
			row.fields[fieldNum].num = fieldNum;
			row.fields[fieldNum].len = 0;
			row.fields[fieldNum].value = NULL;
			row.fields[fieldNum].type = mycb_conf.fieldTypeMap[fieldNum];
			row.fields[fieldNum].typeLen = mycb_conf.fieldLenMap[fieldNum];

			uint bitIndex = fieldNum / 8;
			uint bitPos = fieldNum % 8;

			//如果列不出现在row中
			if (((1 << bitPos) & curPresentBitmap[bitIndex]) == 0) {
				row.fields[fieldNum].isUse = 0;
				continue;
			}

			//如果列为空
			if (((1 << bitPos) & nullBitmap[bitIndex]) != 0) {
				row.fields[fieldNum].len = 0;
				row.fields[fieldNum].value = NULL;
				continue;
			}

			//读取列值
			mycb_read_field(&row.fields[fieldNum]);
		}

		//callback
		//UPDATE_ROWS_EVENTv1 or v2
		event.row = &row;
		event.sql = NULL;
		if (type == ROW_EVENT_UPDATE && (version == 1 || version == 2)) {
			if (curPresentBitmap == presentBitmap) {
				curPresentBitmap = presentBitmap2;
				event.type = EVENT_TYPE_RECORD_DEL;
			} else {
				curPresentBitmap = presentBitmap;
				event.type = EVENT_TYPE_RECORD_ADD;
			}
		} else {
			event.type = type == ROW_EVENT_ADD ? EVENT_TYPE_RECORD_ADD : EVENT_TYPE_RECORD_DEL;
		}
		mycb_conf.eventHandler(&event);

		//清理row
		for (i = 0; i < row.fieldCount; i++) {
			free(row.fields[i].value);
		}
		free(row.fields);
	}

	return 0;
}

int mycb_read_field(field_t * field) {
	switch (field->type) {
	case MYSQLCB_TYPE_YEAR:
		mycb_read_year_field(field);
		break;
	case MYSQLCB_TYPE_DECIMAL:
		mycb_read_var_string_field(field);
		break;
	case MYSQLCB_TYPE_TINY:
		mycb_read_int_field(field, 1);
		break;
	case MYSQLCB_TYPE_SHORT:
		mycb_read_int_field(field, 2);
		break;
	case MYSQLCB_TYPE_INT24:
		mycb_read_int_field(field, 3);
		break;
	case MYSQLCB_TYPE_LONG:
		mycb_read_int_field(field, 4);
		break;
	case MYSQLCB_TYPE_FLOAT:
		mycb_read_float_field(field);
		break;
	case MYSQLCB_TYPE_DOUBLE:
		mycb_read_double_field(field);
		break;
	case MYSQLCB_TYPE_LONGLONG:
		mycb_read_int_field(field, 8);
		break;
	case MYSQLCB_TYPE_NEWDECIMAL:
		mycb_read_var_decimal_field(field);
		break;
	case MYSQLCB_TYPE_VARCHAR:
	case MYSQLCB_TYPE_VAR_STRING:
	case MYSQLCB_TYPE_STRING:
	case MYSQLCB_TYPE_BLOB:
		mycb_read_var_string_field(field);
		break;
	case MYSQLCB_TYPE_SET:
	case MYSQLCB_TYPE_ENUM:
		mycb_read_int_field(field, mycb_conf.fieldLenMap[field->num]);
		break;
	case MYSQLCB_TYPE_DATETIME:
		mycb_read_dateTime_field(field);
		break;
	case MYSQLCB_TYPE_TIME:
		mycb_read_time_field(field);
		break;
	case MYSQLCB_TYPE_DATE:
		mycb_read_date_field(field);
		break;
	case MYSQLCB_TYPE_TIMESTAMP:
		mycb_read_timestamp_field(field);
		break;
	case MYSQLCB_TYPE_BIT:
		mycb_read_bit_field(field);
		break;
	}
	return 0;
}

int mycb_decimal_bin_size(int precision, int scale) {
	int dig2bytes[DIG_PER_DEC1 + 1] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4 };
	int intg = precision - scale, intg0 = intg / DIG_PER_DEC1, frac0 = scale / DIG_PER_DEC1, intg0x = intg
			- intg0 * DIG_PER_DEC1, frac0x = scale - frac0 * DIG_PER_DEC1;
	return intg0 * 4 + dig2bytes[intg0x] + frac0 * 4 + dig2bytes[frac0x];
}

int mycb_read_bit_field(field_t * field) {
	uint i;
	unsigned long long num = 0;
	for (i = 0; i < field->typeLen; i++) {
		uchar tmp = mycb_buf_read_uint8(&mycb_conf.readBuf);
		num = num | (((long long) tmp) << ((field->typeLen - i - 1) * 8));
	}

	char sNum[200] = { 0 };
	snprintf(sNum, sizeof(sNum), "%llu", num);
	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_var_string_field(field_t * field) {
	unsigned long long len = mycb_buf_read_unum(&mycb_conf.readBuf, field->typeLen);
	field->len = len;
	field->value = (char *) malloc(len);
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, field->value, len);
	return 0;
}
int mycb_read_var_decimal_field(field_t * field) {
	field->len = field->typeLen;
	field->value = (char *) malloc(field->len);
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, field->value, field->len);
	bzero(field->value, field->len);
	return 0;
}
int mycb_read_int_field(field_t * field, uint len) {
	long long num = mycb_buf_get_num(&mycb_conf.readBuf, len);
	unsigned long long uNum = mycb_buf_read_unum(&mycb_conf.readBuf, len);

	char sNum[200] = { 0 };
	if (num >= 0) {
		snprintf(sNum, sizeof(sNum), "%lld", num);
	} else {
		snprintf(sNum, sizeof(sNum), "%lld|%llu", num, uNum);
	}
	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_year_field(field_t * field) {
	uint num = mycb_buf_read_uint8(&mycb_conf.readBuf) + 1900;

	char sNum[200] = { 0 };
	snprintf(sNum, sizeof(sNum), "%d", num);
	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_double_field(field_t * field) {
	unsigned long long len = 8;
	char sNum[100] = { 0 };
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, sNum, len);
	double num = *((double *) sNum);
	snprintf(sNum, sizeof(sNum), "%f", num);

	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_float_field(field_t * field) {
	unsigned long long len = 4;
	char sNum[100] = { 0 };
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, sNum, len);
	float num = *((float *) sNum);
	snprintf(sNum, sizeof(sNum), "%f", num);

	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}

int mycb_read_dateTime_field(field_t * field) {
	unsigned long long num = mycb_buf_read_uint64(&mycb_conf.readBuf);

	char sNum[100] = { 0 };
	snprintf(sNum, sizeof(sNum), "%014llu", num);

	struct tm tmpTime;
	strptime(sNum, "%Y%m%d%H%M%S", &tmpTime);
	bzero(sNum, sizeof(sNum));
	strftime(sNum, 24, "%Y-%m-%d %H:%M:%S", &tmpTime);

	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_time_field(field_t * field) {
	unsigned long long num = mycb_buf_read_uint24(&mycb_conf.readBuf);

	char sNum[100] = { 0 };
	snprintf(sNum, sizeof(sNum), "%06llu", num);

	struct tm tmpTime;
	strptime(sNum, "%H%M%S", &tmpTime);
	bzero(sNum, sizeof(sNum));
	strftime(sNum, 24, "%H:%M:%S", &tmpTime);

	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_date_field(field_t * field) {
	unsigned long long num = mycb_buf_read_uint24(&mycb_conf.readBuf);

	uint day = num & 0x1f;
	uint month = (num >> 5) & 0x1f;
	uint year = (num >> 9) & 0x7fff;

	char sNum[100] = { 0 };
	snprintf(sNum, sizeof(sNum), "%u-%u-%u", year, month, day);
	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}
int mycb_read_timestamp_field(field_t * field) {
	unsigned long long num = mycb_buf_read_uint32(&mycb_conf.readBuf);

	char sNum[100] = { 0 };
	snprintf(sNum, sizeof(sNum), "%llu", num);

	field->len = strlen(sNum);
	field->value = (char *) malloc(field->len);
	memcpy(field->value, sNum, field->len);
	return 0;
}

int mycb_parse_rotate_event() {
	unsigned long long position = mycb_buf_read_uint64(&mycb_conf.readBuf);
	uint remain = mycb_conf.readBuf.limit - mycb_conf.readBuf.cur;
	char nextRalayName[remain + 1];
	bzero(nextRalayName, sizeof(nextRalayName));
	mycb_buf_read_fixed_string(&mycb_conf.readBuf, nextRalayName, remain);
	snprintf(mycb_conf.ralayLogFullName, sizeof(mycb_conf.ralayLogFullName), "%s/%s", mycb_conf.ralayLogPath,
			nextRalayName);
	uint ret;
	if ((ret = mycb_switch_ralay_log()) != 0) {
		return -1;
	}

	uint fPos = ftell(mycb_conf.ralayLogFd);
	if (fPos != 0) {
		return 0;
	}

//write file head
	if ((ret = mycb_write_file_head()) != 0) {
		return -1;
	}
	return 0;
}
int mycb_write_file_head() {
	char fileHead[4] = { 0xfe, 0x62, 0x69, 0x6e };
	fwrite(fileHead, sizeof(fileHead), 1, mycb_conf.ralayLogFd);
	if (ferror(mycb_conf.ralayLogFd) > 0) {
		return -1;
	}
	return 0;
}

int mycb_can_read() {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	fd_set fset;
	FD_ZERO(&fset);
	FD_SET(mycb_conf.sock, &fset);
	if (select(mycb_conf.sock + 1, &fset, NULL, NULL, &tv) == 0) {
		return 0;
	}
	return 1;
}
int mycb_read(void *buf, size_t size) {
	int ret;
	size_t remain = size;

	int timeout = 10000;		//超时时间10s
	struct timeval curTv;
	fd_set fset;
	FD_ZERO(&fset);
	FD_SET(mycb_conf.sock, &fset);

	struct timeval maxTv;		//超时时刻
	gettimeofday(&maxTv, NULL );
	maxTv.tv_sec += timeout / 1000;
	maxTv.tv_usec += timeout % 1000 * 1000;

	while (remain > 0) {
		struct timeval remainTv;
		gettimeofday(&curTv, NULL );
		if (curTv.tv_sec > maxTv.tv_sec || (curTv.tv_sec == maxTv.tv_sec && curTv.tv_usec > maxTv.tv_usec)) {
			//超时
			break;
		}

		//新的超时时间
		remainTv.tv_sec = maxTv.tv_sec - curTv.tv_sec;
		remainTv.tv_usec = maxTv.tv_usec - curTv.tv_usec;

		if (select(mycb_conf.sock + 1, &fset, NULL, NULL, &remainTv) == 0) {
			break;
		}
		while (remain > 0) {
			ret = read(mycb_conf.sock, buf, remain);
			if (ret == 0) {
				break;
			}
			if (ret == -1 && errno == EAGAIN) {
				continue;
			}
			remain -= ret;
		}
	}

	if (remain > 0) {
		return -1;
	}
	return 0;
}

int mycb_write(const char *buf, size_t size) {
	int ret;
	size_t remain = size;

	int timeout = 10000;		//超时时间10s
	struct timeval curTv;
	fd_set fset;
	FD_ZERO(&fset);
	FD_SET(mycb_conf.sock, &fset);

	struct timeval maxTv;		//超时时刻
	gettimeofday(&maxTv, NULL );
	maxTv.tv_sec += timeout / 1000;
	maxTv.tv_usec += timeout % 1000 * 1000;

	while (remain > 0) {
		struct timeval remainTv;
		gettimeofday(&curTv, NULL );
		if (curTv.tv_sec > maxTv.tv_sec || (curTv.tv_sec == maxTv.tv_sec && curTv.tv_usec > maxTv.tv_usec)) {
			//超时
			break;
		}

		//新的超时时间
		remainTv.tv_sec = curTv.tv_sec - maxTv.tv_sec;
		remainTv.tv_usec = curTv.tv_usec - maxTv.tv_usec;

		if (select(mycb_conf.sock + 1, &fset, NULL, NULL, &remainTv) == 0) {
			break;
		}
		while (remain > 0) {
			ret = write(mycb_conf.sock, buf, remain);
			if (ret == 0) {
				break;
			}
			if (ret == -1 && errno == EAGAIN) {
				continue;
			}
			remain -= ret;
		}
	}

	if (remain > 0) {
		return -1;
	}
	return 0;
}
