/*
 * mysqlcb_buffer.h
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

#ifndef MYSQLCB_BUFFER_H_
#define MYSQLCB_BUFFER_H_

#define MYCB_BUF_INIT_SIZE 0xff * 0xff * 0xff

#include "mysqlcb.h"

typedef struct s_mycb_buf_t {
	uint size;
	char * limit;
	char * start;
	char * end;
	char * cur;
	char * data;
} mycb_buf_t;

int mycb_buf_init(mycb_buf_t * buf);
void mycb_buf_reset(mycb_buf_t *buf);
ullong mycb_buf_read_unum(mycb_buf_t *buf, uint len);
llong mycb_buf_read_num(mycb_buf_t *buf, uint len) ;

uint mycb_buf_read_uint8(mycb_buf_t *buf) ;
uint mycb_buf_get_uint8(mycb_buf_t *buf);
uint mycb_buf_read_uint16(mycb_buf_t *buf);
uint mycb_buf_read_uint24(mycb_buf_t *buf);
uint mycb_buf_read_uint32(mycb_buf_t *buf) ;
ullong mycb_buf_read_uint48(mycb_buf_t *buf);
ullong mycb_buf_read_uint64(mycb_buf_t *buf) ;
ullong mycb_buf_read_vint(mycb_buf_t *buf);

llong mycb_buf_get_num(mycb_buf_t *buf, uint len);
ullong mycb_buf_get_unum(mycb_buf_t *buf, uint len);

void mycb_buf_write_num(mycb_buf_t *buf, ullong num, uint len);
void mycb_buf_write_uint8(mycb_buf_t *buf, uint num);
void mycb_buf_write_uint16(mycb_buf_t *buf, uint num);
void mycb_buf_write_uint24(mycb_buf_t *buf, uint num);
void mycb_buf_write_uint32(mycb_buf_t *buf, uint num);
void mycb_buf_write_uint64(mycb_buf_t *buf, ullong num) ;

void mycb_buf_write_vint(mycb_buf_t *buf, ullong num) ;
void mycb_buf_write_null_string(mycb_buf_t *buf, const char * data);
void mycb_buf_write_fixed_string(mycb_buf_t *buf, const char * data, int fixedLen);
void mycb_buf_write_var_string(mycb_buf_t *buf, const char * data);

int mycb_buf_read_null_string(mycb_buf_t *buf, char * out);
int mycb_buf_read_fixed_string(mycb_buf_t *buf, char * out, uint len);
int mycb_buf_read_len_string(mycb_buf_t *buf, char * out) ;

uint mycb_buf_get_len(mycb_buf_t *buf);

void mycb_buf_set_buf(mycb_buf_t *buf, char * data, uint len);
void mycb_buf_inc_cur(mycb_buf_t *buf, uint step);

#endif /* MYSQLCB_BUFFER_H_ */
