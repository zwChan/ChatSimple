/*
 * chatSimple.c
 *
 *  Created on: 2015年10月30日
 *      Author: Jason
 */

#include "chatSimple.h"

static struct event_base *base;
int g_dbg = 0;

/**
 * run as a daemon, and redirect the stdin/stdout/stderr to a log file.
 */
int daemonize()
{
    int fd;

    switch (fork()) {
    case -1:
        return (-1);
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        return (-1);

    if ((fd = open("./chatSimple.log", O_APPEND|O_CREAT, 0755)) != -1) {
    	close(STDIN_FILENO);
        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            return (-1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            return (-1);
        }
    }
    return (0);
}

/**
 * handle the event from bufferevent. receive data from client.
 */
void readcb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *input, *output;
    int in_len;
    input = bufferevent_get_input(bev);
    output = bufferevent_get_output(bev);
    in_len = (int)evbuffer_get_length(input);

    trace printf("bufferevent input len=%d\n", in_len);
    while (evbuffer_get_length(input) >= BUFF_MIN_LEN) {
        char buf[BUFF_MIN_LEN];
        int type;
        int len;
    	evbuffer_copyout(input,buf,BUFF_MIN_LEN);
    	type = ntohl(*(int*)buf);
    	len = ntohl(*(int*)(buf+sizeof(int)));
        trace printf("bufferevent msg type=%d, len=%d\n", type, len);

    	if (type > MSG_T_MAX) {
    		msg_response(output,"ERROR: Invalid massage type.");
    		evbuffer_drain(input,evbuffer_get_length(input));
    		return;
    	}
    	if (len > MSG_MAX_LEN) {
    		msg_response(output,"ERROR: Invalid massage length.");
    		evbuffer_drain(input,evbuffer_get_length(input));
    		return;
    	}

    	if (evbuffer_get_length(input) >= BUFF_MIN_LEN + len) {
			MSG *msg = msg_create(type,len, NULL, 0);
			if (msg == NULL) {
				msg_response(output,"ERROR: OOM.");
				evbuffer_drain(input,evbuffer_get_length(input));
				return;
			}
			msg->t = type;
			msg->l = len;
			evbuffer_drain(input, BUFF_MIN_LEN);
			evbuffer_remove(input, msg->p, len);

			procmsg_main(msg, bev, ctx);
			msg_free(&msg);
    	} else {
    		/* wait for the whole msg.*/
    		trace printf("wait for msg. type %d, curr len=%d, wait len = %d\n",
    				type,(int)evbuffer_get_length(input), BUFF_MIN_LEN + len);
    		break;
    	}
    }
}

/**
 * handle the socket error
 */
void errorcb(struct bufferevent *bev, short error, void *ctx)
{
	int fd = bufferevent_getfd(bev);
	USER *user = user_get(NULL, fd);
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        /* ... */
	    trace printf("bufferevent conn BEV_EVENT_EOF. \n");

    } else if (error & BEV_EVENT_ERROR) {
        /* check errno to see what error occurred */
        /* ... */
	    trace printf("bufferevent conn BEV_EVENT_ERROR. \n");

    } else if (error & BEV_EVENT_TIMEOUT) {
        /* must be a timeout event handle, handle it */
        /* ... */
	    trace printf("bufferevent conn BEV_EVENT_TIMEOUT. \n");
    }
    trace printf("bufferevent error. %d\n", error);
    if (user) {
    	user->online = 0;
    	user->intput = NULL;
    	user->output = NULL;
    	user->bev	 = NULL;
    }

    bufferevent_free(bev);
}

/**
 * accept the connection from client. Using bufferevent to handle the new socket.
 */
void do_accept(int listener, short event, void *arg)
{
    struct event_base *base = arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {
        struct bufferevent *bev;
        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, readcb, NULL, errorcb, NULL);
        bufferevent_setwatermark(bev, EV_READ, BUFF_MIN_LEN, BUFF_MAX_LEN);/* at least get type+len.*/
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        trace printf("accept fd=%d\n", fd);
    }
}

/**
 * Create a passive socket to listen the connection from client.
 */
int create_socket(int port) {
    int flags;
    struct sockaddr_in serv_addr;
    int listenfd = 0;
    int error= 0;
    struct linger ling = {0, 0};

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);


    if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0 ||
        fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(listenfd);
        return -1;
    }

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));
    error += setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
    error += setsockopt(listenfd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
    error += setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
    if (error != 0)
        perror("setsockopt fail!\n");

    error = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (error != 0)
        perror("bind fail!\n");
    listen(listenfd, 10);

    return listenfd;

}


/**
 * Main function for the program.
 * Input args:
 * 1. port: listen port
 * 2. -d: run as daemon
 * 3. debug: print debug information
 */
int main (int argc, char *argv[]) {
	int listener;
    struct event *listener_event;
    int port = 3333;

    if (argc > 1) {
    	port = atoi(argv[1]);
        if (port <= 0) {
        	perror("Invalid port.\n");
        	exit(0);
        }
    }

    if (argc > 2 && SAME == strcmp(argv[2],"-d")) {
    	/*make it a background process*/
    	daemonize();
	}
    if (argc > 3 && SAME == strcmp(argv[3],"debug")) {
    	g_dbg = 1;
	}

    /* initialize main thread libevent instance */
	base = event_base_new();
    listener = create_socket(port);

    listener_event = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);
    event_add(listener_event, NULL);

    event_base_dispatch(base);


	return 0;
}
