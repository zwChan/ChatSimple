/*
 * chatSimple.h
 *
 *  Created on: 2015/10/30/
 *      Author: Jason
 */

#ifndef CHATSIMPLE_H_
#define CHATSIMPLE_H_

#define _BSD_SOURCE
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/socket.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>


/*#define NULL ((void *)0L)*/
#define TRUE (1==1)
#define FASLE (0==1)

#define USERNAME_LEN 64
#define SAME 0  /* For strcmp() and memcmp()*/

extern int g_dbg;
#define trace if (g_dbg)

#define USER_DIR "./user/"
#define PATH_LEN 64

/**
 * type of message.
 */
typedef enum MSG_TYPE_tag {
    MSG_T_LOGIN     = 0,    /* user login*/
    MSG_T_TEXT      = 1,    /* send text from a user to another user*/
    MSG_T_FILE      = 2,    /* send a file */
    MSG_T_RESP      = 3,    /* server responses a simple message to client*/

    MSG_T_MAX               /*Keep at the end*/
}MSG_TYPE;

/**
 * user information. a
 */
typedef struct user_info_tag {
    struct user_info_tag *next;
    char name[USERNAME_LEN];
    int fd;
    struct bufferevent *bev;
    struct evbuffer *intput;
    struct evbuffer *output;
    int online;     /* if online or not*/
} USER;

/**
 * The format of massage "on the air". It can be nested.
 * using msg_get_*() to read the value from a tlv.
 *
 *Note: all t/l/ value are in network order.
*/
typedef struct tlv_tag {
    int _t; /*should not be used directly. use macro tlv_type*/
    int _l; /*should not be used directly. use macro tlv_len*/
    char v[1]; /* A pointer to data in msg*/
} TLV;
#define tlv_type(tlv) ntohl((tlv)->_t)
#define tlv_len(tlv) ntohl((tlv)->_l)

/**
 * basic message between client and server.
 * type-length-value structure massage.
 * Usage:
 * Use the msg_create and msg_free in msg.c
 *
 *Note: any value from msg.p is in network order. other fields is host order.
 */
typedef struct msg_tag {
    int t; /*type*/
    int l; /*length*/
    int f; /*flag*/
    int pos; /*current read offset of the value*/
    char *p;   /*pointer of value*/
} MSG;
#define MSG_MAX_LEN (2<<16) /*16K*/
#define MSG_HEAD_LEN (4+4)  /*type + len*/
#define BUFF_MIN_LEN MSG_HEAD_LEN
#define BUFF_MAX_LEN  (MSG_MAX_LEN + MSG_HEAD_LEN)
/*flag of message*/
#define MSG_F_PTR_NEW   (1<<0)   /* msg->p will be freed by msg_free() */
#define MSG_F_PTR_OLD   (1<<1)   /* msg->p will not be freed by msg_free() */
#define MSG_F_PART      (1<<2)   /* the msg is incomplete, because it is too large*/


extern MSG* msg_create(int type, int len, char* v, int flag) ;
extern void msg_response(struct evbuffer *output, char *str);
extern USER* user_get(char *name, int fd);
extern USER* user_add(char *name);
extern void procmsg_main(MSG *msg, struct bufferevent *bev, void *ctx);
extern void msg_free (MSG **msg) ;
extern void user_load(char *dir);
extern int isPathExist (char *path, int isDirOrFile);
extern int get_filesize(char *filename) ;
extern void user_sendOffline(char *name, USER* user);

#endif /* CHATSIMPLE_H_ */
