/**************************************************************
 * comm.h - communication interface for igolgi software
 *
 * Copyright (C) 2009 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: Joshua Horvath (joshua.horvath@igolgi.com)
 **************************************************************/

#ifndef COMM_H 
#define COMM_H 

#include <sys/time.h>

#define MAX_CMD_SIZE    1024
#define MAX_REPLY_SIZE  32768

#define COMM_TIMEOUT        (-2)

#define COMM_CMD_START      0x01
#define COMM_CMD_STOP       0x02
#define COMM_CMD_GETSTATUS  0x03
#define COMM_CMD_GETCONFIG  0x04
#define COMM_CMD_RELOAD     0x05
#define COMM_CMD_QUIT       0x06
#define COMM_CMD_KILL       0x07
#define COMM_CMD_GETVERSION 0x08
#define COMM_CMD_ALIVE      0x09

#define COMM_REPLY_OK           0x01
#define COMM_REPLY_ERR          0x02
#define COMM_REPLY_UNAVAILABLE  0x03
#define COMM_REPLY_INVALID      0x13

#define ERR_INVALID         (-3)
#define ERR_READ            (-4)

typedef struct _comm_obj_struct_
{
    const char *fifo_name;
    // For connectionless UNIX domain sockets
    const char *server_sock_name;
    const char *client_sock_name;
    int fd;
    int fd_tcp_listener;
    
} comm_obj_struct;

#ifdef __cplusplus
extern "C" {
#endif

void *comm_create_fifo(const char *fifo_name);
void *comm_create_unix_domain_sock(const char *sock_name);
void *comm_create_sock_server(int create, int fd);
void *comm_create_sock_client(int create, int fd);
void *comm_create_tcp_server(short port);
void *comm_create_tcp_client(const char* host, short port);
int comm_destroy(void *context);
int comm_connect_unix_domain_sock(void *context, const char *sock_name);
int comm_send_command(void *context, int channel, unsigned char cmd, char *args);
int comm_send_reply(void *context, int channel, char reply, char *msg);
int comm_get_command(void *context, int *channel, char *args, struct timeval *timeout);
int comm_get_reply(void *context, int *channel, char *msg,
                   struct timeval *timeout);

#ifdef __cplusplus
}
#endif

#endif /* COMM_H */

