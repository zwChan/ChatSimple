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
 * remove all the blank character (space and tab) from beginning and end of a string.
 */
char *strtrim(char *line){
	char *end = line + strlen(line)-1;
	while(end > line && (*end==' '||*end=='\t'||*end=='\r'||*end=='\n')) *(end--)='\0';
	while(*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')line++;
	return line;
}

int getFileSize (char *path) {
	struct stat buf;
	/* should not use lstat, we have to follow the symbol file*/
	if (stat(path, &buf) <0) {
		/*printf("stat %s failed (probably file does not exist).\n", path);*/
		return 0;
	}

	if (S_ISDIR(buf.st_mode)) {
		/*printf("not a file: %s", path);*/
		return 0;
	}

	return buf.st_size;
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
    char content[4096*2] = {0};

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

        sleep(1);

        while (1) {
        	usleep(100000);
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

            /*sending file*/
			#define SEND_FILE "send file:"
            {
            	char *line = strtrim(content);
            	if (strncmp(SEND_FILE, line, strlen(SEND_FILE))==SAME) {
        			char buff[4096];
        			int n = 0;
        			char *filename = strtrim(content+strlen(SEND_FILE));
                	char *filename_nopath=filename+strlen(filename);
        			int filesize;
        			int sentsize=0;
        			int fd = open(filename,O_RDONLY);
        			if (fd<0){
        				fprintf(stderr,"file can't not open, %s\n", filename);
        				continue;
        			}
        			filesize = getFileSize(filename);
        			while(*(filename_nopath-1) != '/' && filename != filename_nopath)filename_nopath--;

            		while (1) {
						n = read(fd, buff,4096);
						if (n==0) {
							/*completed*/
							/*break;  we have to send a len==0 msg to indicate file reach end.*/
						}else if (n<0 ) {
							if (errno == EAGAIN)continue;
							perror("send file error.\n");
							break;
						}

	        			/*send user name, file name first*/
	        			type = htonl(MSG_T_FILE);
						len = htonl(MSG_HEAD_LEN * 3 + strlen(username) + 1 + strlen(filename_nopath) + 1 + n);
						write(sockfd, &type, 4);
						write(sockfd, &len, 4);

						type = htonl(22);
						len = htonl(strlen(username) + 1);
						write(sockfd, &type, 4);
						write(sockfd, &len, 4);
						write(sockfd, username, ntohl(len));

						type = htonl(33);
						len = htonl(strlen(filename_nopath) + 1);
						write(sockfd, &type, 4);
						write(sockfd, &len, 4);
						write(sockfd, filename_nopath, ntohl(len));

						type = htonl(44);
						len = htonl(n);
						write(sockfd, &type, 4);
						write(sockfd, &len, 4);
						if (n>0) {
							write(sockfd,buff,n);
							sentsize += n;
							printf("\r%.2f%% completed.    ", 1.0*sentsize/filesize*100);
						}else{
							/*completed*/
							printf("\nsend file successfully.\n");
							break;
						}
					}
                	continue;
            	}
            }
            type = htonl(MSG_T_TEXT);
            len = htonl(MSG_HEAD_LEN * 2 + strlen(username) + 1 + strlen(content) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);
            /*user name*/
            type = htonl(22);
            len = htonl(strlen(username) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);
            write(sockfd, username, ntohl(len));
            /*content*/
            type = htonl(23);
            len = htonl(strlen(content) + 1);
            write(sockfd, &type, 4);
            write(sockfd, &len, 4);
            write(sockfd, content, ntohl(len));
            if (errno != 0) {
                perror("some thing wrong.\n");
                exit(0);
            }
/*            sleep(1);*/

        }
    } else {
        /* child, for read msg*/
        int rcvfd = -1;
    	int type;
        int len;
        char filename[256]={0};
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
            } else if (type == MSG_T_FILE) {
                /*user anme*/
            	read(sockfd, &type, 4);
                read(sockfd, &len, 4);
                len = ntohl(len);
                read(sockfd, username, len);
                trace printf("rcv username %s, len %d\n", username, len);
                /*filename*/
                read(sockfd, &type, 4);
                read(sockfd, &len, 4);
                len = ntohl(len);
                read(sockfd, filename, len);
                trace printf("rcv filename %s, len %d, rcvfd %d\n", filename, len, rcvfd);
                if (rcvfd<0){
                	/*open file*/
                	int tempCnt=0;
                	/*file the not exist file*/
                	while(getFileSize(filename)>0){
                		filename[len-1]=0; /*recover to original file name*/
                		sprintf(filename,"%s(%d)",filename,++tempCnt);
                	}
                	errno = 0; /*drop the errno produce by previous system call*/
                    printf("\n>Receiving file [%s] from [%s]:", filename, username);
                	rcvfd = open(filename, O_CREAT|O_WRONLY, 0644);
                	if (rcvfd<0){
						fprintf(stderr,"file can't not create, %s\n", filename);
						/*FIXME: notify server receiving abort.*/
						continue;
					}
                }
                read(sockfd, &type, 4);
                read(sockfd, &len, 4);
                len = ntohl(len);
                trace printf("rcv content, len %d\n", len);
                if (len==0) {
                	close(rcvfd);
                	rcvfd = -1;
                    printf("\n>file received successfully.");
                }else{
					read(sockfd, content, len);
					write(rcvfd,content,len);
                }
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
            }else {
                read(sockfd, content, len);
                printf(">[unknown] %s\n", content);
            }
            if (errno != 0) {
                perror("\nsome thing wrong.\n");
                exit(0);
            }
            fflush(stdout);
        }
    }


    if (errno != 0) {
        perror("some on client wrong.\n");
    }
    return 0;
}
