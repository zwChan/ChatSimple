/*
 * client.c
 *
 *  Created on: 2015/11/1
 *      Author: Jason
 */

#include "chatSimple.h"

#define LINE_LEN 1024
int g_dbg = 0;

void printconnerror()
{
    switch (errno) {
        case ETIMEDOUT : printf("Connection timed out.\n"); break;
        case ECONNREFUSED : printf("Connection refused.\n"); break;
        case EHOSTDOWN : printf("Host down.\n"); break;
        case EHOSTUNREACH : printf("No route to the host.\n"); break;
        case ENETUNREACH : printf("Network unreachable.\n"); break;
        default: printf("errno = %d\n", errno);
    }
}

/**
 * entry of client
 * options:
 * 1. server ip:
 * 2. server port
 * 3. user name
 * 4. debug
 */
int main(int argc, char * argv[])
{
    int sockfd;
    struct sockaddr_in addr;
    char username[LINE_LEN] = {0};
    char content[LINE_LEN] = {0};

    if (argc < 3) {
        printf("Usage: a.out ip_addr port username.\n");
        exit(0);
    }
    if (argc > 4 && SAME == strcmp(argv[4],"debug")) {
        g_dbg = 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror(": Can't get socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printconnerror();
        perror(": connect");
        exit(1);
    }
    trace printf("connect to %s, port %s, user = %s\n", argv[1], argv[2], argv[3]);

    if (fork() > 0){
        /* parent, write msg*/
        /* login the user*/
        /*msg tlv*/
        int type = 0;
        int len = strlen(argv[3])+1;
        int total = MSG_HEAD_LEN + len;

        sleep(1);

        type = ntohl(type);
        total = ntohl(total);
        write(sockfd, &type, 4);
        write(sockfd, &total, 4);
        /*username tlv*/
        type = ntohl(21);
        len = htonl(len);
        write(sockfd, &type, 4);
        write(sockfd, &len, 4);
        write(sockfd, argv[3], strlen(argv[3])+1);
        if (errno != 0) {
        	perror("error on login.\n");
        }

        while (1) {
            printf("*To user: ");
            scanf("%s", username);

            printf("*content, a line: ");
            /*scanf("%s", content);*/
            fgets(content, LINE_LEN, stdin);  /*eat a \r\n, ugly, I know.*/
            if (NULL == fgets(content, LINE_LEN, stdin)){
            	trace printf("fgets get null.\n");
            	break;
            }
            content[LINE_LEN-1] = '\0';

            type = htonl(MSG_T_TEXT);
            len = htonl(MSG_HEAD_LEN * 2 + strlen(username) + 1 + strlen(content) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);

            type = htonl(22);
            len = htonl(strlen(username) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);
            write(sockfd, username, ntohl(len));

            type = htonl(23);
            len = htonl(strlen(content) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);
            write(sockfd, content, ntohl(len));
            if (errno != 0) {
                perror("some thing wrong.\n");
                exit(0);
            }
            sleep(1);

        }
    } else {
        /* child, for read msg*/
        int type;
        int len;
        errno = 0;
        while (1) {
            int ret = 0;
            ret = read(sockfd, &type,4);
            if (ret<=0) break;
            read(sockfd, &len,4);
            type = ntohl(type);
            len = ntohl(len);
            trace printf("rcv type %d, len %d\n", type, len);
            if (type == MSG_T_RESP) {
                read(sockfd, content, len);
                printf(">%s\n",content);
            } else if (type == MSG_T_TEXT) {
                read(sockfd, &type, 4);
                read(sockfd, &len, 4);
                len = ntohl(len);
                read(sockfd, username, len);
                printf("\n>[%s]:", username);

                read(sockfd, &type, 4);
                read(sockfd, &len, 4);
                len = ntohl(len);
                read(sockfd, content, len);
                printf("\n>%s", content);
                fflush(stdout);
            } else {
                read(sockfd, content, len);
                printf(">[unknown] %s\n", content);
            }
            if (errno != 0) {
                perror("\nsome thing wrong.\n");
                exit(0);
            }
        }
    }


    if (errno != 0) {
        perror("some on client wrong.\n");
    }
    return 0;
}
