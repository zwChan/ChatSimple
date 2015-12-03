/*
 * msg.c
 *
 *  Created on: 2015/11/1/
 *      Author: Jason
 */

#include "chatSimple.h"

typedef int (*pf_msg_cb)(MSG *msg, USER *user, void *data);

typedef struct msg_cb_tag {
    MSG_TYPE type;
    pf_msg_cb cb;
} MSG_CB;


/**
 *  #################### msg basic operation begin ###########
 */
/**
 * create a new message.
 * Flag:
 *  MSG_F_PTR_NEW: hand the input 'v' to msg directly, it will be freed when msg_del();
 *  MSG_F_PTR_OLD: hand the input 'v' to msg directly; it will not be freed when msg_del();
 *  else: it will malloc for 'v' and copy the input 'v' to message.
 */
MSG* msg_create(int type, int len, char* v, int flag) {
    MSG *msg;
    msg = malloc(sizeof(MSG));
    if (!msg) {
        perror("OOM\n");
        return NULL;
    }
    msg->t = type;
    msg->l = len;
    msg->f=flag;
    msg->pos = 0;
    if ((flag & MSG_F_PTR_NEW) || (flag & MSG_F_PTR_NEW)) {
        msg->p = v;
    } else {
        msg->p = malloc(len);
        if (!msg->p) {
            free(msg);
            return NULL;
        }
        if (v) {
            memcpy(msg->p, v, len);
        }
    }
    return msg;
}

void msg_free (MSG **msg) {
    if ((*msg)->p && 0 == ((*msg)->f & MSG_F_PTR_OLD)) {
        free((*msg)->p);
    }
    free(*msg);
    *msg = NULL;
}

/**
 * Simply response a string to client. Mainly for error massage.
 */
void msg_response(struct evbuffer *output, char *str) {
    int type = htonl(MSG_T_RESP);
    int len = htonl(strlen(str)+1);
    evbuffer_add(output, &type, 4);
    evbuffer_add(output, &len, 4);
    evbuffer_add(output, str, strlen(str)+1);
    return;
}

/**
 * reset the offset of the message, so that we can read it from beginning.
 */
void msg_reset(MSG *msg) {
    msg->pos = 0;
}
/**
 * read a string from the msg. the string is a TLV format data in the msg.
 */
char *msg_get_string(MSG *msg) {
    TLV *tlv = (TLV*)(msg->p+msg->pos);
    msg->pos += MSG_HEAD_LEN + tlv_len(tlv);
    return tlv->v;
}
/**
 * read a integer from the msg. it is a TLV format data in the msg.
 */
int msg_get_int(MSG *msg) {
    TLV *tlv = (TLV*)(msg->p+msg->pos);
    msg->pos += MSG_HEAD_LEN + tlv_len(tlv);
    return ntohl(*(int*)(void*)(tlv->v));
}

/**
 * read a short integer from the msg. it is a TLV format data in the msg.
 */
short msg_get_short(MSG *msg) {
    TLV *tlv = (TLV*)(msg->p+msg->pos);
    msg->pos += MSG_HEAD_LEN + tlv_len(tlv);
    return ntohs(*(short*)(void*)(tlv->v));
}

/**
 * read a sub-message from the current msg. it is a TLV format data in the msg.
 */
MSG *msg_get_submsg(MSG *msg) {
    TLV *tlv = (TLV*)(msg->p+msg->pos);
    msg->pos += MSG_HEAD_LEN + tlv_len(tlv);
    return msg_create(tlv_type(tlv), tlv_len(tlv), tlv->v, MSG_F_PTR_OLD);
}


/**
 *  #################### msg basic operation end ###########
 */

/**
 *  #################### msg callback operation begin ###########
 */

void promsg_send(USER *user, void *buff, int len) {
	if (user->online) {
		evbuffer_add(user->output, buff, len);
	}else{
		/*open file if it is not open*/
		if (user->fd <= 0) {
			char filename[PATH_LEN];
			sprintf(filename,"%s%s", USER_DIR,user->name);
			user->fd = open(filename, O_WRONLY);
			/*write(user->fd,buff,len);*/
			if (user->fd <= 0) {
				perror("open user file fail.\n");
				return;
			}
		}
		write(user->fd,buff,len);
		trace printf("user %s get off line msg, len %d\n", user->name, len);

	}
}
/**
 * user login
 */
int procmsg_login(MSG *msg, USER *user, void *data) {
    struct evbuffer *input  = bufferevent_get_input(data);
    struct evbuffer *output = bufferevent_get_output(data);
    static char buff_short[256];
    static char buff[1024] ;
    USER *u = g_users;
    int cnt=0;
    int userCnt = 0;
    while (u) {
        if ( (u->online)) {
        	userCnt++;
        	sprintf(buff_short,"Welcome %s online!", user->name);
        	msg_response(u->output, buff_short);
        	cnt += sprintf(buff+cnt, "%s,", u->name);
        }
        u = u->next;
    }

    user->online = 1;
    user->intput = input;
    user->output = output;
    if (user->fd > 0) {
    	/*close the local file*/
    	close(user->fd);
    }
    user->fd = bufferevent_getfd(data);

    sprintf(buff_short, "Hello! %s. There are %d users online:[%s]",
    		user->name, userCnt, buff);
    msg_response(output,buff_short);

    user_sendOffline(user->name,user);

    trace printf("procmsg_login %s\n", user->name);

    return 0;
}

/**
 *  receive a msg and send to another user
 */
int procmsg_text(MSG *msg, USER *user, void *data) {
    char *toUserName, *content;
    USER * toUser;
    toUserName = msg_get_string(msg);
    content = msg_get_string(msg);
    trace printf("procmsg_text from user %s toUser %s, content %s\n", user->name, toUserName, content);

    toUser = user_get(toUserName,0);
    if (!toUser) {
        msg_response(user->output, "ERROR: user not found.");
        return 1;
    }
    if (!toUser->online) {
        msg_response(user->output, "Warning: user not online, cache on server.");
    }

    {
        int type = htonl(MSG_T_TEXT);
        int len = htonl(MSG_HEAD_LEN * 2 + strlen(user->name) + 1 + strlen(content) + 1);
        promsg_send(toUser, &type, 4);
        promsg_send(toUser, &len, 4);

        type = htonl(11);
        len = htonl(strlen(user->name) + 1);
        promsg_send(toUser, &type, 4);
        promsg_send(toUser, &len, 4);
        promsg_send(toUser, user->name, ntohl(len));

        type = htonl(12);
        len = htonl(strlen(content) + 1);
        promsg_send(toUser,&type, 4);
        promsg_send(toUser,&len, 4);
        promsg_send(toUser,content, ntohl(len));
    }

    /*msg_response(user->output, "you message have been delivered.");*/
    trace printf("procmsg_text from %s to %s, len=%d\n", user->name, toUser->name, msg->l);

    return 0;
}

/**
 *  receive a msg and send to another user
 */
int procmsg_file(MSG *msg, USER *user, void *data) {
    char *toUserName;
    USER * toUser;
    toUserName = msg_get_string(msg);
    trace printf("procmsg_file from user %s toUser %s, len %d\n",user->name, toUserName, msg->l);

    toUser = user_get(toUserName,0);
    if (!toUser) {
        msg_response(user->output, "ERROR: user not found.");
        return 1;
    }
    if (!toUser->online) {
        msg_response(user->output, "Error: user not online, can not send file.");
        return 1;
    }

    {
        int type = htonl(MSG_T_FILE);
        int len_user = 0;
        int len_all = htonl(msg->l + strlen(user->name) - strlen(toUser->name));
        promsg_send(toUser, &type, 4);
        promsg_send(toUser, &len_all, 4);

        type = htonl(11);
        len_user = htonl(strlen(user->name) + 1);
        promsg_send(toUser, &type, 4);
        promsg_send(toUser, &len_user, 4);
        promsg_send(toUser, user->name, ntohl(len_user));

        /*forward the reset msg, include file name tlv and content tlv, for now*/
        promsg_send(toUser,msg->p+msg->pos, ntohl(len_all) - (MSG_HEAD_LEN+ntohl(len_user)));
    }

    /*msg_response(user->output, "you message have been delivered.");*/
    trace printf("procmsg_file from %s to %s, len=%d\n", user->name, toUser->name, msg->l);

    return 0;
}

/**
 *  #################### msg callback operation end ###########
 */

/**
 * registration for the msg handle callback.
 */
MSG_CB msg_cb[] = {
    {MSG_T_LOGIN,   procmsg_login},
    {MSG_T_TEXT,    procmsg_text},
    {MSG_T_FILE,    procmsg_file},

};

void procmsg_main(MSG *msg, struct bufferevent *bev, void *ctx) {
    struct evbuffer *input,*output;
    USER *user;
    int ret = 0;
    int fd = bufferevent_getfd(bev);
    output = bufferevent_get_output(bev);
    input = bufferevent_get_input(bev);

    trace printf("procmsg_main: get msg type=%d, len=%d\n", msg->t,msg->l);

    if (msg->t == MSG_T_LOGIN) {
        /* find user or create user*/
        char *name = msg_get_string(msg);
        user = user_get(name,0);
        if (!user) {
            user = user_add(name);
            if (!user) {
            	trace printf("add user fail %s\n", name);
                return;
            }
         }
        if (user->online == 1) {
            msg_response(output,"ERROR: user already online.");
            trace printf("user already online %s\n", user->name);
            /*bufferevent_flush(bev,EV_WRITE,BEV_FINISHED);*/
            /*close(fd);*/
            bufferevent_free(bev);
            return;
        }
    } else {
        user = user_get(NULL,fd);
    }

    trace printf("get msg from %s, type=%d, len=%d\n", user->name, msg->t,msg->l);

    msg_reset(msg);
    ret = msg_cb[msg->t].cb(msg, user, bev);
    if (ret) {
        trace printf("type %d ret = %d\n", msg->t, ret);
        evbuffer_drain(input,evbuffer_get_length(input));
    }
}
