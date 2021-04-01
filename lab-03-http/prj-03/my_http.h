#ifndef _MY_HTTP_H
#define _MY_HTTP_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NAME_SZ 0x200
#define MSG_SZ 0x1000
#define MAX_SESSION 10
#define SPORT 80

void decode_input(char *url, char *dhost, char *dpath);
void *receiver(void *cs);

#endif