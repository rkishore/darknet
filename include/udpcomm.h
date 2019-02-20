/**************************************************************
 * udpcomm.h - PRESTO: Igolgi A/V Transcoder Application
 *
 * Copyright (C) 2009, 2010, 2011 Igolgi Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: John Richardson (john.richardson@igolgi.com)
 **************************************************************/

#if !defined(_IGOLGI_UDP_COMM_H_)
#define _IGOLGI_UDP_COMM_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_UDP_BUFFER          1500
#define MAX_TIMEOUT_MSG_SERVER  1000
#define MAX_TIMEOUT_MSG_CLIENT  500
#define MAX_TIMEOUT_DATA        250
#define REQUEST_TOKEN           0x13
#define RESPONSE_TOKEN          0x14
#define GETSTATUS_TOKEN         0x15
#define GETCONFIG_TOKEN         0x16

#define MAX_CMD_SIZE            1024
#define MAX_REPLY_SIZE          32768

#define COMM_TIMEOUT            (-2)

#define COMM_CMD_START          0x01
#define COMM_CMD_STOP           0x02
#define COMM_CMD_GETSTATUS      0x03
#define COMM_CMD_GETCONFIG      0x04
#define COMM_CMD_RELOAD         0x05
#define COMM_CMD_QUIT           0x06
#define COMM_CMD_KILL           0x07
#define COMM_CMD_GETVERSION     0x08

#define COMM_REPLY_OK           0x01
#define COMM_REPLY_ERR          0x02
#define COMM_REPLY_INVALID      0x13

#define ERR_INVALID             (-3)
#define ERR_READ                (-4)

#define HDRELAY_CHILD_COMM_PORT    70000
#define HDRELAY_SERVER_COMM_PORT   (HDRELAY_CHILD_COMM_PORT+MAX_CLASSIFY_CHILDREN*10+10)

#if defined(__cplusplus)
extern "C" {
#endif

  int udp_wait_comm_socket(int current_socket, int timeout, fd_set *trigger);
  int udp_read_comm_socket(int current_socket, uint8_t *buffer, int max_buffer_size, int *channel, int client_udp_port, int *daemonport);
  int udp_send_comm_socket(int current_socket, uint8_t *buffer, int buffer_size, int channel, int client_udp_port, int server_udp_port, int *daemonport);
  int udp_server_send_message(int current_socket, int message, int channel, char *auxdata, int auxdata_size, int *daemonport);
  int udp_client_send_message_response(int current_socket, int msg_response, int my_channel, int *daemonport);
  int udp_server_get_message_response(int current_socket, int *which_channel, int *daemonport);

  int udp_server_get_data_response(int current_socket, 
				   uint8_t *output_buffer_status, 
				   uint8_t *output_buffer_config,
				   int *which_channel,
				   int *token,
				   int *daemonport);

  int udp_client_send_data_response(int current_socket, int msg_response, uint8_t *data_response, int data_response_len, int my_channel, int token, int *daemonport);
  int udp_client_get_message(int current_socket, int channel, char *auxdata, int *auxdata_size, int *daemonport);
  int udp_open_comm_socket(const char *address, int udp_port, int channel);
  void udp_cleardata(void);

#if defined(__cplusplus)
}
#endif

#endif // _IGOLGI_UDP_COMM_H_
