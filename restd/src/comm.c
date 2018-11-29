/**************************************************************
 * comm.c - communication interface for igolgi software
 *
 * Copyright (C) 2009 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: Joshua Horvath (joshua.horvath@igolgi.com)
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <netinet/tcp.h>

#include "comm.h"

void *comm_create_fifo(const char *fifo_name)
{
    comm_obj_struct *obj;

    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating fifo communications interface\n");
        return NULL;
    }

    obj->fd_tcp_listener = -1;
    obj->fifo_name = fifo_name;
    obj->fd = open(obj->fifo_name, O_RDWR);
    if (obj->fd == -1)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, could not open fifo\n");
        return NULL;
    }

    return (void *)obj;
}

void *comm_create_unix_domain_sock(const char *sock_name)
{
    comm_obj_struct *obj;
    struct sockaddr_un address;
    int socket_fd;
    size_t address_length;

    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket communications interface\n");
        return NULL;
    }

    obj->fd_tcp_listener = -1;
    obj->client_sock_name = sock_name;
    obj->fifo_name = NULL;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket\n");
        return NULL;
    } 

    // Remove any existing socket path
    unlink(sock_name);
    address.sun_family = AF_UNIX;
    address_length = sizeof(address.sun_family) + sprintf(address.sun_path, "%s", sock_name);

    if (bind(socket_fd, (struct sockaddr *)&address, address_length) != 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] bind() failed\n");
        close(socket_fd);
        return NULL;
    }

    obj->fd = socket_fd;

    return (void *)obj;
}

int comm_connect_unix_domain_sock(void *context, const char *sock_name)
{
    comm_obj_struct *obj;

    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        return -1;
    }

    if (sock_name == obj->client_sock_name)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, client and server sockets are the same\n");
        return -1;
    }

    obj->server_sock_name = sock_name;

    return 0;
}

void *comm_create_sock_server(int create, int fd)
{
    comm_obj_struct *obj;

    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket communications interface\n");
        return NULL;
    }

    obj->fd_tcp_listener = -1;
    obj->fifo_name = NULL;
    obj->client_sock_name = NULL;
    obj->server_sock_name = NULL;

    if (!create)
    {
        // Just use the existing socket
        obj->fd = fd;
        return (void *)obj;
    }

    // TODO: create the socket server
    return (void *)obj;
}

void *comm_create_sock_client(int create, int fd)
{
    comm_obj_struct *obj;

    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket communications interface\n");
        return NULL;
    }

    obj->fd_tcp_listener = -1;
    obj->fifo_name = NULL;
    obj->client_sock_name = NULL;
    obj->server_sock_name = NULL;

    if (!create)
    {
        // Just use the existing socket
        obj->fd = fd;
        return (void *)obj;
    }

    // TODO: create the socket client 
    return (void *)obj;
}

void *comm_create_tcp_server(short port)
{
    comm_obj_struct *obj;
    struct sockaddr_in address;
    int socket_fd;
    int opt;
    int ret;
    int flag;
    
    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket communications interface\n");
        return NULL;
    }

    obj->fd = -1;
    obj->client_sock_name = NULL;
    obj->fifo_name = NULL;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating TCP socket\n");
        return NULL;
    } 
    
    opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) 
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error setting TCP sockopt");
        return NULL;
    }    

    flag = 1;
    ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(struct sockaddr)) != 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] TCP bind() failed\n");
        close(socket_fd);
        return NULL;
    }
    
    if (listen(socket_fd, 0) != 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] listen() failed\n");
        close(socket_fd);
        return NULL;
    }

    obj->fd_tcp_listener = socket_fd;

    return (void *)obj;
}

void *comm_create_tcp_client(const char* host, short port)
{
    comm_obj_struct *obj;
    struct sockaddr_in address;
    int socket_fd;
    struct hostent *h;
    int flag;
    int ret;
    
    obj = (comm_obj_struct *)malloc(sizeof(comm_obj_struct));
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket communications interface\n");
        return NULL;
    }

    obj->fd = -1;
    obj->client_sock_name = NULL;
    obj->fifo_name = NULL;

    h = gethostbyname(host);
    if (!h) 
    {
        syslog(LOG_WARNING, "[MPEG2TS] Failed to resolve client host\n");
        return NULL;    
    }
    
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating socket\n");
        return NULL;
    } 

    flag = 1;
    ret = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    
    address.sin_family = AF_INET;
    memcpy(&address.sin_addr.s_addr, h->h_addr, h->h_length);    
    address.sin_port = htons(port);
    
    if (connect(socket_fd, (struct sockaddr *)&address, sizeof(struct sockaddr)) != 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] TCP connect() failed\n");
        close(socket_fd);
        return NULL;
    }
    
    obj->fd = socket_fd;

    return (void *)obj;
}

int comm_destroy(void *context)
{
    comm_obj_struct *obj;

    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        return -1;
    }

    if (obj->fd != -1)
    {
        close(obj->fd);
    }
    
    if (obj->fd_tcp_listener != -1)
    {
        close(obj->fd_tcp_listener);
    }

    if (obj->client_sock_name)
    {
        unlink(obj->client_sock_name);
    }

    free(obj);
    return 0;
}

int comm_send_command(void *context, int channel, unsigned char cmd, char *args)
{
    comm_obj_struct *obj;
    char buf[MAX_CMD_SIZE];
    int offset = 0;
    int cmd_len;
    int ret;
    
    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_command called with NULL obj");
        syslog(LOG_WARNING, "[MPEG2TS] Error, comm_send_command, comm object is NULL\n");
        return -1;
    }

    switch (cmd)
    {
    case COMM_CMD_START:
        offset = sprintf(buf, "PUT START\n");
        break;
    case COMM_CMD_STOP:
        offset = sprintf(buf, "PUT STOP\n");
        break;
    case COMM_CMD_GETSTATUS:
        offset = sprintf(buf, "GET STATUS\n");
        break;
    case COMM_CMD_GETCONFIG:
        offset = sprintf(buf, "GET CONFIG\n");
        break;
    case COMM_CMD_GETVERSION:
        offset = sprintf(buf, "GET VERSION\n");
        break;
    case COMM_CMD_RELOAD:
        offset = sprintf(buf, "PUT RELOAD\n");
        break;
    case COMM_CMD_QUIT:
        offset = sprintf(buf, "PUT QUIT\n");
        break;
    case COMM_CMD_KILL:
        offset = sprintf(buf, "PUT KILL\n");
        break;
    default:
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_command called with unknown command (%d)", cmd);
        syslog(LOG_WARNING, "[MPEG2TS] Unknown command\n");
        return -1;
    }

    if (offset < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_command: offset < 0\n");
        return -1;
    }

    if (channel == -1)
    {
        ret = sprintf(buf + offset, "Channel: all\n");
    }
    else
    {
        ret = sprintf(buf + offset, "Channel: %d\n", channel);
    }

    if (ret < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_command: ret < 0\n");
        return -1;
    }

    if (args)
    {
        if (args[0] != 0)
        {
            strcat(buf, args);
        }
    }

    cmd_len = strlen(buf) + 1;

    if (obj->client_sock_name)
    {
        struct sockaddr_un servaddr;

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sun_family = AF_UNIX;
        strcpy(servaddr.sun_path, obj->server_sock_name);
        ret = sendto(obj->fd, buf, cmd_len, 0, (struct sockaddr *)&servaddr,
                     sizeof(servaddr));
    }
    else
    {
        ret = write(obj->fd, buf, cmd_len);
    }

    if (ret != cmd_len)
    {
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_command: ret != cmd_len\n");
        return -1;
    }

    return 0;
}

int comm_get_command(void *context, int *channel, char *args, struct timeval *timeout)
{
    comm_obj_struct *obj;
    char buf[MAX_CMD_SIZE];
    const char *delim = "\n";
    char *line;
    int cmd;
    int parsed_channel = -1;
    int ret;
    fd_set rfds;
    char *saveptr;

    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, comm_get_command, comm object is NULL\n");
        return -1;
    }

    if (channel)
    {
        *channel = -1;
    }
    
    // For TCP, accept a connection to get the command
    if (obj->fd_tcp_listener != -1)
    {        
        if (obj->fd != -1) 
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error, get_command called while connection still active.");
            close(obj->fd);
            obj->fd = -1;
            return -1;
        }
        
        FD_ZERO(&rfds);
        FD_SET(obj->fd_tcp_listener, &rfds);
        
        ret = select(obj->fd_tcp_listener + 1, &rfds, NULL, NULL, timeout);
        if (ret == -1)
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error on accept select()\n");
            return -1;
        }
        else if (ret == 0)
        {
            return COMM_TIMEOUT;
        }
        
        obj->fd = accept(obj->fd_tcp_listener, NULL, NULL);
    } 

    if (timeout && obj->fd_tcp_listener == -1)
    {
        FD_ZERO(&rfds);
        FD_SET(obj->fd, &rfds);
        
        ret = select(obj->fd + 1, &rfds, NULL, NULL, timeout);
        if (ret == -1)
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error on recv select()\n");
            return -1;
        }
        else if (ret == 0)
        {
            return COMM_TIMEOUT;
        }
    }

    if (obj->client_sock_name)
    {
        struct sockaddr_un from;
        socklen_t fromlen;

        fromlen = sizeof(from);
        ret = recvfrom(obj->fd, buf, MAX_CMD_SIZE, 0, (struct sockaddr *)&from,
                       &fromlen);
        buf[ret] = 0;
    }
    else
    {
        ret = read(obj->fd, buf, MAX_CMD_SIZE);
        buf[ret] = 0;
    }

    if (ret == -1)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error reading command\n");
        return ERR_READ;
    }

    line = strtok_r(buf, delim, &saveptr);
    if (line == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, invalid command\n");
        return ERR_INVALID;
    }

    if (!strcmp(line, "PUT START"))
    {
        cmd = COMM_CMD_START;
    }
    else if (!strcmp(line, "PUT STOP"))
    {
        cmd = COMM_CMD_STOP;
    }
    else if (!strcmp(line, "GET STATUS"))
    {
        cmd = COMM_CMD_GETSTATUS;
    }
    else if (!strcmp(line, "GET CONFIG"))
    {
        cmd = COMM_CMD_GETCONFIG;
    }
    else if (!strcmp(line, "GET VERSION"))
    {
        cmd = COMM_CMD_GETVERSION;
    }
    else if (!strcmp(line, "PUT RELOAD"))
    {
        cmd = COMM_CMD_RELOAD;
    }
    else if (!strcmp(line, "PUT QUIT"))
    {
        cmd = COMM_CMD_QUIT;
    }
    else if (!strcmp(line, "PUT KILL"))
    {
        cmd = COMM_CMD_KILL;
    }
    else
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, invalid command\n");
        return ERR_INVALID;
    }

    // Parse optional parameters
    line = strtok_r(NULL, delim, &saveptr);
    while (line)
    {
        if (strstr(line, "Channel:"))
        {
            if (!strcmp(line, "Channel: all"))
            {
                parsed_channel = -1;
            }
            else
            {
                if (sscanf(line, "Channel: %d", &parsed_channel) != 1)
                {
                    syslog(LOG_WARNING, "[MPEG2TS] Error, invalid channel\n");
                    return ERR_INVALID;
                }
            }

            if (channel)
            {
                *channel = parsed_channel;
            }
        }
        else
        {
            if (args)
            {
                strcat(args, line);
            }
        }
        line = strtok_r(NULL, delim, &saveptr);
    }

    return cmd;
}

int comm_send_reply(void *context, int channel, char reply, char *msg)
{
    comm_obj_struct *obj;
    char buf[MAX_REPLY_SIZE];
    int reply_len;
    int offset = 0;
    int ret;
    
    memset(buf, 0, sizeof(buf));

    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, comm_send_reply, comm object is NULL\n");
        return -1;
    }

    if (obj->fd == -1)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, attempt to send reply without connection\n");
        return -1;
    }
    
    switch (reply)
    {
    case COMM_REPLY_INVALID:
        offset = sprintf(buf, "400 Bad Request\n");
        break;

    case COMM_REPLY_OK:
        offset = sprintf(buf, "200 OK\n");
        break;

    case COMM_REPLY_ERR:
        offset = sprintf(buf, "500 Internal Server Error\n");
        break;

    case COMM_TIMEOUT:
        offset = sprintf(buf, "408 Request Timeout\n");
        break;
    
    default:
        syslog(LOG_WARNING, "[MPEG2TS] comm_send_reply called with unknown reply: %d\n", reply);
        
    }

    if (offset < 0)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error creating reply\n");
        return -1;
    }

    if (channel != -1)
    {
        ret = sprintf(buf + offset, "Channel: %d\n", channel);
        if (ret < 0)
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error creating reply\n");
            return -1;
        }
    }
 
    if (msg)
    {
        int msg_len = strlen(msg) + 1;

        if (strlen(buf) + msg_len + 1 > MAX_REPLY_SIZE)
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error, reply message exceeds maximum length\n");
            return -1;
        }

        strcat(buf, msg);
        strcat(buf, "\n");
    }

    reply_len = strlen(buf) + 1;

    if (obj->client_sock_name)
    {
        struct sockaddr_un servaddr;

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sun_family = AF_UNIX;
        strcpy(servaddr.sun_path, obj->server_sock_name);

        ret = sendto(obj->fd, buf, reply_len, 0, (struct sockaddr *)&servaddr,
                     sizeof(servaddr));
    }
    else
    {
        ret = write(obj->fd, buf, reply_len);
    }

    if (ret != reply_len)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error sending reply: ret = %d\n", ret);
        return -1;
    }
    
    // If TCP mode, close the active connection
    if (obj->fd_tcp_listener != -1) 
    {
        close(obj->fd);
        obj->fd = -1;
    }

    return 0;
}

int comm_get_reply(void *context, int *channel, char *msg,
                   struct timeval *timeout)
{
    comm_obj_struct *obj;
    char buf[MAX_REPLY_SIZE];
    const char *delim = "\n";
    char *line;
    int reply;
    int parsed_channel = -1;
    int ret;
    fd_set rfds;
    char *saveptr;

    memset(buf, 0, sizeof(buf));

    obj = (comm_obj_struct *)context;
    if (obj == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, comm_get_reply, comm object is NULL\n");
        return COMM_REPLY_ERR;
    }

    if (channel)
    {
        *channel = -1;
    }

    if (timeout)
    {
        FD_ZERO(&rfds);
        FD_SET(obj->fd, &rfds);

        ret = select(obj->fd + 1, &rfds, NULL, NULL, timeout);
        if (ret == -1)
        {
            syslog(LOG_WARNING, "[MPEG2TS] Error on select()\n");
            return COMM_REPLY_ERR;
        }
        else if (ret == 0)
        {
            syslog(LOG_WARNING, "[MPEG2TS] comm_get_reply: COMM_TIMEOUT\n");
        
            return COMM_TIMEOUT;
        }
    }

    if (obj->client_sock_name)
    {
        struct sockaddr_un from;
        socklen_t fromlen;

        fromlen = sizeof(from);
        ret = recvfrom(obj->fd, buf, MAX_REPLY_SIZE, 0,
                       (struct sockaddr *)&from, &fromlen);
        buf[ret] = 0;
    }
    else
    {
        ret = read(obj->fd, buf, MAX_REPLY_SIZE);
        buf[ret] = 0;
    }

    if (ret == -1)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error reading reply\n");
        return COMM_REPLY_ERR;
    }

    if (msg)
    {
        char *body_ptr;
        body_ptr = strchr(buf, '\n');
        if (body_ptr)
        {
            body_ptr++;
            strcpy(msg, body_ptr);
        }
    }

    line = strtok_r(buf, delim, &saveptr);
    if (line == NULL)
    {
        syslog(LOG_WARNING, "[MPEG2TS] Error, invalid reply\n");
        return COMM_REPLY_ERR;
    }

    if (strstr(line, "400"))
    {      
        reply = COMM_REPLY_INVALID;
    } 
    else if (strstr(line, "200"))
    {
        reply = COMM_REPLY_OK;
    }
    else if (strstr(line, "408"))
    {
        reply = COMM_TIMEOUT;
    }
    else
    {
        reply = COMM_REPLY_ERR;
    }

    // Parse optional parameters
    line = strtok_r(NULL, delim, &saveptr);
    while (line)
    {
        if (strstr(line, "Channel:"))
        {
            if (sscanf(line, "Channel: %d", &parsed_channel) != 1)
            {
                syslog(LOG_WARNING, "[MPEG2TS] Error, invalid channel\n");
                return COMM_REPLY_ERR;
            }

            if (channel)
            {
                *channel = parsed_channel;
            }
        }
        line = strtok_r(NULL, delim, &saveptr);
    }

    return reply;
}

