/*
 * mysqlcb.h
 *
 *  Created on: 2012-11-18
 *      Author: quanwei
 */

#ifndef MYSQLCB_DEF_H_
#define MYSQLCB_DEF_H_

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

#define FORMAT_DESCRIPTION_EVENT 0x0f

//mysql data type
#define MYSQLCB_TYPE_DECIMAL  0x00
#define MYSQLCB_TYPE_TINY  0x01
#define MYSQLCB_TYPE_SHORT  0x02
#define MYSQLCB_TYPE_LONG  0x03
#define MYSQLCB_TYPE_FLOAT  0x04
#define MYSQLCB_TYPE_DOUBLE  0x05
#define MYSQLCB_TYPE_NULL  0x06
#define MYSQLCB_TYPE_TIMESTAMP  0x07
#define MYSQLCB_TYPE_LONGLONG  0x08
#define MYSQLCB_TYPE_INT24  0x09
#define MYSQLCB_TYPE_DATE  0x0a
#define MYSQLCB_TYPE_TIME  0x0b
#define MYSQLCB_TYPE_DATETIME  0x0c
#define MYSQLCB_TYPE_YEAR  0x0d
#define MYSQLCB_TYPE_NEWDATE  0x0e
#define MYSQLCB_TYPE_VARCHAR  0x0f
#define MYSQLCB_TYPE_BIT  0x10
#define MYSQLCB_TYPE_NEWDECIMAL  0xf6
#define MYSQLCB_TYPE_ENUM  0xf7
#define MYSQLCB_TYPE_SET  0xf8
#define MYSQLCB_TYPE_TINY_BLOB  0xf9
#define MYSQLCB_TYPE_MEDIUM_BLOB  0xfa
#define MYSQLCB_TYPE_LONG_BLOB  0xfb
#define MYSQLCB_TYPE_BLOB  0xfc
#define MYSQLCB_TYPE_VAR_STRING  0xfd
#define MYSQLCB_TYPE_STRING  0xfe
#define MYSQLCB_TYPE_GEOMETRY  0xff

#define EVENT_TYPE_SQL_EXEC 1
#define EVENT_TYPE_RECORD_ADD 2
#define EVENT_TYPE_RECORD_DEL 3

#define ROW_EVENT_ADD 1
#define ROW_EVENT_UPDATE 2
#define ROW_EVENT_DELETE 3

#define DIG_PER_DEC1 9

typedef unsigned char uchar;
typedef unsigned long long ullong;
typedef long long llong;

#endif /* MYSQLCB_DEF_H_ */
