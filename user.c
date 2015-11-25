/*
 * user.c
 *
 *  Created on: 2015/11/1
 *      Author: Jason
 */
#include "chatSimple.h"

USER *g_users = NULL;

USER* user_get(char *name, int fd) {
    USER *user = g_users;
    while (user) {
        if ( (name && SAME == strcmp(name, user->name)) ||
                (fd>0 && user->fd == fd)) {
            return user;
        }
        user = user->next;
    }
    trace printf("user get fail. name:%s,fd:%d\n", name, fd);
    return NULL;
}

USER* user_add(char *name) {
    USER *user = user_get(name,0);
    if (!user) {
    	char filename[PATH_LEN];
        user = malloc(sizeof(USER));
        memset(user,0,sizeof(USER));
        if (!user) return NULL;
        strcpy(user->name, name);
        user->next = g_users;
        g_users = user;
        sprintf(filename, "%s%s", USER_DIR, name);
        if (!isPathExist(filename,0)) {
        	int fd = open(filename,O_CREAT,0644);
        	close(fd);
        }
        trace printf("user add. name:%s\n", name);
    }
    return user;
}

void user_del(char *name) {
    USER *user = g_users;
    /*check first*/
    if (user) {
        if (SAME == strcmp(name, user->name)) {
            g_users = user->next;
            free(user);
            return;
        }
    } else {
        return;
    }
    while (user->next) {
        if (SAME == strcmp(name, user->next->name)) {
            user->next = user->next->next;
            return;
        }
        user = user->next;
    }
    trace printf("user del. name:%s\n", name);
    return;
}

/**
 * isDirOrFile : 0: not care; 1:dir; 2:file
 */
int isPathExist (char *path, int isDirOrFile) {
	struct stat buf;

	/* should not use lstat, we have to follow the symbol file*/
	if (stat(path, &buf) <0) {
		trace printf("stat %s failed (probably file does not exist).\n", path);
		return 0;
	}

	if (isDirOrFile==1 && !S_ISDIR(buf.st_mode)) {
		return 0;
	}
	if (isDirOrFile==2 && S_ISDIR(buf.st_mode)) {
		return 0;
	}
	return 1;
}

int get_filesize(char *filename) {
	struct stat st;
	if (stat(filename, &st) == 0) {
		return st.st_size;
	}
	return 0;
}
/**
 * Load user name from the user directory
 */
void user_load(char *dir)
{
  DIR *d;
  char userFileName[64];
  struct dirent *w;
  struct stat buf;

  trace printf("start load user from dir %s\n", dir);
  if ((d = opendir(dir)) == NULL) {
    int no = errno;
    if (stat(dir, &buf) == -1) {
      printf("Directory %s does not exist.\n", dir);
      return ;
    } else {
      printf("%s: %s\n", dir, strerror(no));
      return ;
    }
  }

  while ((w = readdir(d)) != NULL) {
	sprintf(userFileName, "%s%s", dir, w->d_name);
	if (stat(userFileName, &buf) == -1 || S_ISDIR(buf.st_mode)) {
    	continue;
    }
    user_add(w->d_name);
    trace printf("load user from local. name:%s\n", w->d_name);
  }
  closedir(d);
  return;
}

void user_sendOffline(char *name, USER* user) {
	char filename[PATH_LEN];
	char buff[1024];
	int fd;
	int fsize = 0;
	FILE *f;
	sprintf(filename, "%s%s", USER_DIR, name);
	if (!isPathExist(filename,0)) {
		trace printf("user has no data file. name:%s\n", name);
		return;
	}

	sleep(1); /*XXX */

	/*send off line msg to the user*/
	if ((fsize = get_filesize(filename)) > 0) {
		ssize_t  cnt = 0;
		fd = open(filename,O_RDONLY);
		if (fd < 0) {
			perror(filename);
			return;
		}
		trace printf("user %s has off line msg, len %d\n", user->name, fsize);
		while (1) {
			cnt = read(fd,buff,1024);
			if (cnt < 0) {
				if (errno == EAGAIN) {
					continue;
				}else{
					perror("read offline msg fail.\n");
					break;
				}
			}else if (cnt == 0) {
				break;
			}

			trace printf("send off line msg to %s, len %d\n", user->name, (int)cnt);
			evbuffer_add(user->output, buff, cnt);
			fsize -= cnt;
			if (fsize<=0) {
				break;
			}
		}

		close(fd);
		/*return;*/
	}


	/*clean the old msg*/
	f = fopen(filename, "w");
	if (!f) {
		perror("clear user file fail.\n");
		return;
	}
	fclose(f);

	trace printf("send off line msg finished. name:%s\n", name);
}
