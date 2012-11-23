/*
 * mysqlcb.h
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

#ifndef MYSQLCB_H_
#define MYSQLCB_H_

#include <assert.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>

#include "mysql/mysql.h"

#include "mysqlcb_def.h"
#include "mysqlcb_buffer.h"

typedef struct s_field_t {
	unsigned int isUse;
	unsigned int num;
	unsigned int type;
	unsigned int typeLen;
	unsigned int len;
	char *value;
} field_t;

typedef struct s_row_t {
	unsigned int fieldCount;
	field_t * fields;
} row_t;

typedef struct s_event_t {
	unsigned int type;
	unsigned int time;
	char *db;
	char *table;
	unsigned int len;
	row_t *row;
	char * sql;
} event_t;

typedef struct s_mycb_conf_t {
	char host[256]; //master host
	int port; //master port
	char userName[256]; //master用户名
	char password[256]; //master密码
	int serverId; //slave server id
	char ralayLogPath[256]; //ralay log 目录
	char ralayLogName[256]; //ralay log 文件名
	char ralayLogFullName[256]; //ralay log 完整文件名
	int ralayLogPos; //ralay log 文件尾
	int sock; //mysql socket
	MYSQL *mysql; //mysql资源
	FILE * ralayLogFd; //ralay log 文件句柄
	mycb_buf_t readBuf; //读缓冲
	mycb_buf_t writeBuf; //写缓冲
	void (*eventHandler)(event_t * event);
	uchar fieldTypeMap[256];
	uint fieldLenMap[256];
	char db[256];
	char table[256];
	uint eventTime;//事件产生的时间
	uint verbose;
	uint isRun;//
} mycb_conf_t;

extern mycb_conf_t mycb_conf;

/*
 * mysqlcb.c
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

void mycb_init();
int mycb_start();
int mycb_connect();
int mycb_init_ralay_log();
int mycb_load_max_ralay_from_db(char * maxFile, uint len);
int mycb_send_dump_cmd();
void mycb_print_error_packet();
int mycb_loop_event();
int mycb_open_ralay_log();
int mycb_switch_ralay_log();
int mycb_write_packet(uint sid);
int mycb_read_packet();
int mycb_write_to_ralay_file();
int mycb_parse_event();
int mycb_parse_desc_event();
int mycb_parse_query_event();
int mycb_parse_tablemap_event();
int mycb_parse_rows_event(uint type, uint version);
int mycb_read_field(field_t * field);
int mycb_decimal_bin_size(int precision, int scale);
int mycb_read_bit_field(field_t * field);
int mycb_read_var_string_field(field_t * field);
int mycb_read_var_decimal_field(field_t * field);
int mycb_read_int_field(field_t * field, uint len);
int mycb_read_year_field(field_t * field);
int mycb_read_double_field(field_t * field);
int mycb_read_float_field(field_t * field);
int mycb_read_dateTime_field(field_t * field);
int mycb_read_time_field(field_t * field);
int mycb_read_date_field(field_t * field);
int mycb_read_timestamp_field(field_t * field);
int mycb_parse_rotate_event();
int mycb_write_file_head();
int mycb_can_read();
int mycb_read(void *buf, size_t size);
int mycb_write(const char * buf, size_t size);

void startMysqlcb(const char *host, int port, const char *userName, const char * password, int serverId,
		const char * ralayLogPath, void (*eventHandler)(event_t *));

#endif /* MYSQLCB_H_ */
