/*
 * mysqlcb_buffer.c
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

#ifndef MYSQLCB_BUFFER_C_
#define MYSQLCB_BUFFER_C_

#include "mysqlcb.h"
#include "mysqlcb_buffer.h"

int mycb_buf_init(mycb_buf_t * buf) {
	assert(buf != NULL);

	buf->data = (char *) malloc(MYCB_BUF_INIT_SIZE);
	assert(buf->data != NULL);

	buf->start = buf->data;
	buf->size = MYCB_BUF_INIT_SIZE;
	mycb_buf_reset(buf);

	return 0;
}

void mycb_buf_reset(mycb_buf_t *buf) {
	buf->start = buf->data;
	buf->cur = buf->start;
	buf->end = buf->start + buf->size;
	buf->limit = buf->start;
}

ullong mycb_buf_read_unum(mycb_buf_t *buf, uint len) {
	ullong num = 0;
	uint i = 0;

	for (; i < len; i++) {
		uchar tmp = *(buf->cur);
		buf->cur++;
		num = num | (((long long) tmp) << (i * 8));
	}
	return num;
}

llong mycb_buf_read_num(mycb_buf_t *buf, uint len) {
	long long num = 0;
	uint i = 0;

	for (; i < len; i++) {
		char tmp = *(buf->cur);
		buf->cur++;
		num = num | (((long long) tmp) << (i * 8));
	}

	switch (len) {
	case 1:
		num = (char) num;
		break;
	case 2:
		num = (short) num;
		break;
	case 4:
		num = (int) num;
		break;
	case 8:
		num = (long long) num;
		break;
	}
	return num;
}

ullong mycb_buf_read_vint(mycb_buf_t *buf) {
	unsigned long long num = 0;
	uchar first;
	first = *(buf->cur);
	buf->cur++;
	if (first < 0xfb) {
		num = first;
	} else if (first == 0xfc) {
		num = mycb_buf_read_uint16(buf);
	} else if (first == 0xfd) {
		num = mycb_buf_read_uint24(buf);
	} else if (first == 0xfe) {
		num = mycb_buf_read_uint64(buf);
	}
	return num;
}
llong mycb_buf_get_num(mycb_buf_t *buf, uint len) {
	llong num = mycb_buf_read_num(buf, len);
	buf->cur -= len;
	return num;
}
ullong mycb_buf_get_unum(mycb_buf_t *buf, uint len) {
	ullong num = mycb_buf_read_unum(buf, len);
	buf->cur -= len;
	return num;
}

uint mycb_buf_read_uint8(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 1);
	return num;
}
uint mycb_buf_get_uint8(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 1);
	buf->cur--;
	return num;
}
uint mycb_buf_read_uint16(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 2);
	return num;
}
uint mycb_buf_read_uint24(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 3);
	return num;
}
uint mycb_buf_read_uint32(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 4);
	return num;
}
ullong mycb_buf_read_uint48(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 6);
	return num;
}
ullong mycb_buf_read_uint64(mycb_buf_t *buf) {
	ullong num = mycb_buf_read_unum(buf, 8);
	return num;
}

void mycb_buf_write_num(mycb_buf_t *buf, ullong num, uint len) {
	uchar tmp;
	uint i = 0;

	for (; i < len; i++) {
		tmp = (num >> (i * 8)) & 0xff;
		memcpy(buf->cur, &tmp, 1);
		buf->cur++;
	}
}
void mycb_buf_write_uint8(mycb_buf_t *buf, uint num) {
	mycb_buf_write_num(buf, num, 1);
}
void mycb_buf_write_uint16(mycb_buf_t *buf, uint num) {
	mycb_buf_write_num(buf, num, 2);
}
void mycb_buf_write_uint24(mycb_buf_t *buf, uint num) {
	mycb_buf_write_num(buf, num, 3);
}
void mycb_buf_write_uint32(mycb_buf_t *buf, uint num) {
	mycb_buf_write_num(buf, num, 4);
}
void mycb_buf_write_uint64(mycb_buf_t *buf, ullong num) {
	mycb_buf_write_num(buf, num, 8);
}

void mycb_buf_write_vint(mycb_buf_t *buf, ullong num) {
	uchar tmp;
	uchar first;
	if (num < 251) {
		first = (char) num;
		memcpy(buf->cur++, &first, 1);
	}
	if (num >= 251 && num < (2L << 16)) {
		first = (uchar) 0xfc;
		memcpy(buf->cur++, &first, 1);
		tmp = (uchar) num & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 8) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
	}
	if (num >= (2L << 16) - 1 && num < (2L << 24)) {
		first = (uchar) 0xfd;
		memcpy(buf->cur++, &first, 1);
		tmp = (uchar) num & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 8) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 16) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
	}
	if (num >= (2L << 24) - 1 && num < (2L << 64)) {
		first = (uchar) 0xfe;
		memcpy(buf->cur++, &first, 1);
		tmp = (uchar) num & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 8) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 16) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 24) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 32) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 40) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 48) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
		tmp = (uchar) (num >> 56) & 0xff;
		memcpy(buf->cur++, &tmp, 1);
	}
}

void mycb_buf_write_null_string(mycb_buf_t *buf, const char * data) {
	int len = strlen(data) + 1;
	memcpy(buf->cur, data, len);
	buf->cur += len;
}
void mycb_buf_write_fixed_string(mycb_buf_t *buf, const char * data, int fixedLen) {
	int len = strlen(data) + 1;
	memcpy(buf->cur, data, len);
	int remainLen = fixedLen - len;
	if (remainLen > 0) {
		char fill[remainLen];
		bzero(fill, remainLen);
		memcpy(buf, fill, remainLen);
	}
	buf->cur += fixedLen;
}
void mycb_buf_write_var_string(mycb_buf_t *buf, const char * data) {
	int len = strlen(data) + 1;
	mycb_buf_write_vint(buf, len);
	memcpy(buf->cur, data, len + 1);
	buf->cur += len;
}

int mycb_buf_read_null_string(mycb_buf_t *buf, char * out) {
	return -1;
}
int mycb_buf_read_fixed_string(mycb_buf_t *buf, char * out, uint len) {
	memcpy(out, buf->cur, len);
	buf->cur += len;
	return len;
}
int mycb_buf_read_len_string(mycb_buf_t *buf, char * out) {
	uint len = mycb_buf_read_vint(buf);
	return mycb_buf_read_fixed_string(buf, out, len);
}

uint mycb_buf_get_len(mycb_buf_t *buf) {
	return buf->cur - buf->start;
}

void mycb_buf_set_buf(mycb_buf_t *buf, char * data, uint len) {
	mycb_buf_reset(buf);
	assert(len < buf->end-buf->start);
	memcpy(buf->start, data, len);
	buf->limit = buf->start + len;
}
void mycb_buf_inc_cur(mycb_buf_t *buf, uint step) {
	buf->cur += step;
}
#endif /* MYSQLCB_BUFFER_C_ */
