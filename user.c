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
		user = malloc(sizeof(USER));
		if (!user) return NULL;
		strcpy(user->name, name);
		user->next = g_users;
		g_users = user;
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
