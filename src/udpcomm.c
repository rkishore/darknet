/**************************************************************
 * udpcomm.c - PRESTO: Igolgi A/V Transcoder Application
 *
 * Copyright (C) 2009, 2010, 2011 Igolgi Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: John Richardson (john.richardson@igolgi.com)
 **************************************************************/

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <linux/filter.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

#include "udpcomm.h"

static uint8_t last_config_data[MAX_CLASSIFY_CHILDREN][MAX_UDP_BUFFER];
static uint8_t last_status_data[MAX_CLASSIFY_CHILDREN][MAX_UDP_BUFFER];

void udp_cleardata(void)
{ 
    memset(last_config_data, 0, sizeof(last_config_data));
    memset(last_status_data, 0, sizeof(last_status_data));
}

int udp_wait_comm_socket(int current_socket, int timeout, fd_set *trigger)
{
    struct timeval timeout_data;
    int max_value = current_socket;
    int retval;

    FD_ZERO(trigger);
    FD_SET(current_socket, trigger);  

    timeout_data.tv_sec = timeout / 1000;
    timeout_data.tv_usec = 1000 * (timeout % 1000);

    retval = select(max_value + 1, trigger, NULL, NULL, &timeout_data);
    if (retval < 0) {
        syslog(LOG_ERR,"UDP_WAIT_COMM_SOCKET: unable to setup select\n");
        FD_ZERO(trigger);
        return -1;
    }
    return retval;
}

int udp_read_comm_socket(int current_socket, uint8_t *buffer, int max_buffer_size, int *channel, int client_udp_port, int *daemonport)
{
    int bytes_read;
    struct sockaddr_in src_addr;
    int addrlen = sizeof(struct sockaddr_in);
    int recv_port = 0;
    int recv_channel = -1;

    bytes_read = recvfrom(current_socket, (void*)buffer, max_buffer_size - 1, 0, (struct sockaddr *)&src_addr, (socklen_t*)&addrlen);
    if (bytes_read > 0) {
        recv_port = ntohs(src_addr.sin_port);
	
        if (channel) {

	  
            recv_channel = (recv_port - client_udp_port - *daemonport);
            if (recv_channel >= 0) {
                recv_channel = recv_channel / 10;
            } else {
                recv_channel = -1;
            }        
        }
    }
    //syslog(LOG_INFO,"UDP_READ_COMM_SOCKET: RECEIVED BUFFER: %d FROM PORT: %d on CHANNEL: %d\n", bytes_read, recv_port, recv_channel);

    if (channel) {
        *channel = recv_channel;
    }

    return bytes_read;
}

int udp_send_comm_socket(int current_socket, uint8_t *buffer, int buffer_size, int channel, int client_udp_port, int server_udp_port, int *daemonport)
{
    struct sockaddr_in dst_addr;
    struct in_addr caddr;
    int addrlen = sizeof(struct sockaddr_in);
    int dstport = client_udp_port + (channel*10) + *daemonport;
    int data_sent;

    if (channel >= 1000) {
        dstport = server_udp_port + ((channel-1000)*10) + *daemonport;  // SEND TO THE SERVER
    }

    if (current_socket > 0) {    
        inet_aton("127.0.0.1", &caddr);
        dst_addr.sin_family = AF_INET;
        dst_addr.sin_addr.s_addr = caddr.s_addr;
        dst_addr.sin_port = htons(dstport);

        //syslog(LOG_INFO,"UDP_SEND_COMM_SOCKET:  SENDING BUFFER: %d TO PORT %d\n", buffer_size, dstport);
        data_sent = sendto(current_socket, buffer, buffer_size, 0, (struct sockaddr *)&dst_addr, addrlen);
        //syslog(LOG_INFO,"UDP_SEND_COMM_SOCKET:  SENT %d BYTES TO PORT %d\n", data_sent, dstport);

        return data_sent;
    }
    syslog(LOG_ERR,"UDP_SEND_COMM_SOCKET: invalid socket\n");
    return -1;
}

int udp_server_send_message(int current_socket, int message, int channel, char *auxdata, int auxdata_size, int *daemonport)
{
    uint8_t message_data[MAX_UDP_BUFFER];
    int message_size = 5;
    int retval;

    if (current_socket > 0) {
        message_data[0] = 0x10;
        message_data[1] = 0x67;
        message_data[2] = REQUEST_TOKEN;
        message_data[3] = REQUEST_TOKEN;
        message_data[4] = message;

        if (auxdata_size > 0 && (auxdata_size < MAX_UDP_BUFFER - 5)) {
            message_size += auxdata_size;
            memcpy(&message_data[5], auxdata, auxdata_size);
        }

        //syslog(LOG_INFO,"UDP_SERVER_SEND_MESSAGE: sending message: %d to channel: %d\n", message, channel);
        retval = udp_send_comm_socket(current_socket, (uint8_t*)&message_data[0], message_size, channel, HDRELAY_CHILD_COMM_PORT, HDRELAY_SERVER_COMM_PORT, daemonport);
        //syslog(LOG_INFO,"UDP_SERVER_SEND_MESSAGE: sent message: %d to channel: %d retval: %d\n", message, channel, retval);
        
        return retval;
    }
    syslog(LOG_ERR,"UDP_SERVER_SEND_MESSAGE: invalid socket\n");
    return -1;
}

int udp_client_send_message_response(int current_socket, int msg_response, int my_channel, int *daemonport)
{
    uint8_t message_data[MAX_UDP_BUFFER];
    int message_size = 5;
    int retval;

    if (current_socket > 0) {
        message_data[0] = 0x10;
        message_data[1] = 0x67;
        message_data[2] = RESPONSE_TOKEN;
        message_data[3] = RESPONSE_TOKEN;
        message_data[4] = msg_response;

        //syslog(LOG_INFO,"UDP_CLIENT_SEND_MESSAGE: sending message: %d to SERVER\n", msg_response);
        retval = udp_send_comm_socket(current_socket, (uint8_t*)&message_data[0], message_size, 1000+my_channel, HDRELAY_CHILD_COMM_PORT, HDRELAY_SERVER_COMM_PORT, daemonport);
        //syslog(LOG_INFO,"UDP_CLIENT_SEND_MESSAGE: sent message: %d to SERVER retval: %d\n", msg_response, retval);

        return retval;         
    }
    syslog(LOG_ERR,"UDP_CLIENT_SEND_MESSAGE_RESPONSE: invalid socket\n");
    return -1;
}

int udp_server_get_message_response(int current_socket, int *which_channel, int *daemonport)
{
    int retval;
    fd_set msg_trigger;
    int data_read;
    uint8_t saved_buffer[MAX_UDP_BUFFER];
    int max_buffer_size = MAX_UDP_BUFFER;
    int bad_apples = 0;

    if (current_socket > 0 && which_channel) {
check_message_again:
        retval = udp_wait_comm_socket(current_socket, MAX_TIMEOUT_MSG_SERVER, &msg_trigger);     
   
        if (retval < 0) {
            // ERROR
            syslog(LOG_ERR,"SERVER_GET_MESSAGE: SOCKET CLOSED: SELECT FAILED\n");
            return ERR_READ;
        }
        if (retval == 0) {
            //syslog(LOG_ERR,"[CH=%d] TIMEOUT WAITING FOR MESSAGE\n", channel);
            // TIMEOUT
            return COMM_TIMEOUT;
        }

        if (retval > 0) {
            if (FD_ISSET(current_socket, &msg_trigger)) {
	        data_read = udp_read_comm_socket(current_socket, (uint8_t*)saved_buffer, max_buffer_size, which_channel, HDRELAY_CHILD_COMM_PORT, daemonport);
                if (data_read < 0) {
                    syslog(LOG_ERR,"SERVER_GET_MESSAGE: SOCKET CLOSED: READ FAILED\n");
                    return ERR_READ;
                }
                if (data_read > 0) {                  
                    uint8_t *p_message_buffer;
                    int which_command = ERR_READ;

                    syslog(LOG_INFO,"SERVER_GET_MESSAGE: RECEIVED MESSAGE FROM CLIENT : %d\n", *which_channel);
                
                    if (saved_buffer[0] == 0x10 && 
                        saved_buffer[1] == 0x67 && 
                        saved_buffer[2] == RESPONSE_TOKEN && 
                        saved_buffer[3] == RESPONSE_TOKEN) {
                        which_command = saved_buffer[4];
                        if ((which_command > 0 || which_command < 3) || (which_command == 0x13)) {
                            //======== THE COMMAND IS VALID =========
                            return which_command;
                        } else {
                            which_command = ERR_READ;
                        }
                    } else {
                        // LEFT OVER DATA IN THE SOCKET QUEUE
                        syslog(LOG_INFO,"SERVER_GET_MESSAGE:  RECEIVED A DIFFERENT MESSAGE : 0x%x 0x%x 0x%x 0x%x  CLIENT : %d\n", 
                               saved_buffer[0],
                               saved_buffer[1],
                               saved_buffer[2],
                               saved_buffer[3],
                               *which_channel);
                        bad_apples++;
                        if (bad_apples > 3) {
                            return COMM_REPLY_ERR;
                        }
                        goto check_message_again;
                    }
                }
            }
        }
    }
    if (which_channel) {
        *which_channel = -1;
    }

    return ERR_READ;
}

int udp_server_get_data_response(int current_socket, 
                                 uint8_t *output_buffer_status, 
                                 uint8_t *output_buffer_config,
                                 int *which_channel,
                                 int *token,
				 int *daemonport)
{
    int retval;
    fd_set msg_trigger;
    int data_read;
    uint8_t saved_buffer[MAX_UDP_BUFFER];
    int max_buffer_size = MAX_UDP_BUFFER;
    char channel_name[64];
    int channel_name_size = 0;
    memset(channel_name, 0, sizeof(channel_name));
    int bad_apples = 0;

    if (current_socket > 0 && which_channel) {
check_message_again:
        retval = udp_wait_comm_socket(current_socket, MAX_TIMEOUT_DATA, &msg_trigger);   
        if (retval < 0) {         
            syslog(LOG_ERR,"SERVER_GET_DATA: SOCKET CLOSED: SELECT FAILED\n");
            return ERR_READ;
        }

        if (retval == 0) { 
            return COMM_TIMEOUT;
        }        

        if (retval > 0) {
            if (FD_ISSET(current_socket, &msg_trigger)) {
	        data_read = udp_read_comm_socket(current_socket, (uint8_t*)saved_buffer, max_buffer_size, which_channel, HDRELAY_CHILD_COMM_PORT, daemonport);
                if (data_read < 0) {
                    syslog(LOG_ERR,"SERVER_GET_DATA: SOCKET CLOSED: READ FAILED\n");
                    return ERR_READ;
                }
                if (data_read > 0) {
                    uint8_t *p_message_buffer;                
                
                    if (saved_buffer[0] == 0x10 && 
                        saved_buffer[1] == 0x67) {
                        if (saved_buffer[2] == GETSTATUS_TOKEN &&
                            saved_buffer[3] == GETSTATUS_TOKEN) {
                            //syslog(LOG_INFO,"[MASTER]  SERVER_GET_MESSAGE: RECEIVED GETSTATUS MESSAGE FROM CLIENT : %d\n", *which_channel);

                            sprintf(channel_name,"Channel: %d\n", *which_channel);
                            channel_name_size = strlen(channel_name);

                            // TODO
                            memset(output_buffer_status, 0, 8192);
                            memcpy(output_buffer_status, channel_name, channel_name_size);
                            memcpy(output_buffer_status + channel_name_size, &saved_buffer[4], data_read - 4);

                            *token = GETSTATUS_TOKEN;
                            return COMM_REPLY_OK;
                        } else if (saved_buffer[2] == GETCONFIG_TOKEN &&
                                   saved_buffer[3] == GETCONFIG_TOKEN) {
                            //syslog(LOG_INFO,"[MASTER]  SERVER_GET_MESSAGE: RECEIVED GETCONFIG MESSAGE FROM CLIENT : %d\n", *which_channel);

                            sprintf(channel_name,"Channel: %d\n", *which_channel);
                            channel_name_size = strlen(channel_name);

                            // TODO
                            memset(output_buffer_config, 0, 8192);
                            memcpy(output_buffer_config, channel_name, channel_name_size);
                            memcpy(output_buffer_config + channel_name_size, &saved_buffer[4], data_read - 4);

                            *token = GETCONFIG_TOKEN;
                            return COMM_REPLY_OK;
                        } else {
                            syslog(LOG_INFO,"[MASTER]  SERVER_GET_MESSAGE: RECEIVED UNKNOWN MESSAGE TOKEN FROM CLIENT : %d  (IGNORING)\n", *which_channel);
                            return COMM_TIMEOUT;
                        }
                    } else {
                        syslog(LOG_INFO,"[MASTER]  SERVER_GET_MESSAGE: RECEIVED UNKNOWN MESSAGE FORMAT FROM CLIENT : %d  (IGNORING)\n", *which_channel);
                        return COMM_TIMEOUT;
                    }
                } else {
                    syslog(LOG_INFO,"[MASTER]  SERVER_GET_MESSAGE: UNABLE TO RECEIVE MESSAGE FROM CLIENT (FATAL ERROR)\n");
                    return ERR_READ;
                }
            }
        }        
    }
    if (which_channel) {
        *which_channel = -1;
    }
    return ERR_READ;
}

int udp_client_send_data_response(int current_socket, int msg_response, uint8_t *data_response, int data_response_len, int my_channel, int token, int *daemonport)
{
    uint8_t message_data[MAX_UDP_BUFFER];
    int message_size = 0;
    int retval;

    if (current_socket > 0) {
        message_data[0] = 0x10;
        message_data[1] = 0x67;
        message_data[2] = token;
        message_data[3] = token;
        message_size = 4;

        if (data_response_len > MAX_UDP_BUFFER - 4) {
            data_response_len = MAX_UDP_BUFFER - 4;
        }
        memcpy(&message_data[4], data_response, data_response_len);
        message_size += data_response_len;

        retval = udp_send_comm_socket(current_socket, (uint8_t*)&message_data[0], message_size, 1000+my_channel, HDRELAY_CHILD_COMM_PORT, HDRELAY_SERVER_COMM_PORT, daemonport);

        return retval;         
    }
    syslog(LOG_ERR,"UDP_CLIENT_SEND_MESSAGE_RESPONSE: invalid socket\n");


 
    return 0;
}

int udp_client_get_message(int current_socket, int channel, char *auxdata, int *auxdata_size, int *daemonport)
{  
    int retval;
    fd_set msg_trigger;
    int data_read;
    uint8_t saved_buffer[MAX_UDP_BUFFER];
    int max_buffer_size = MAX_UDP_BUFFER;
    int i;

    //===== THIS IS RECEIVING ON UDP PORT 55000+(channel*10) ON 127.0.0.1 ==========

    //syslog(LOG_INFO,"UDP_CLIENT_GET_MESSAGE : WAITING - CHANNEL : %d\n", channel);
    retval = udp_wait_comm_socket(current_socket, MAX_TIMEOUT_MSG_CLIENT, &msg_trigger);

    if (retval < 0) {
        // ERROR
        syslog(LOG_ERR,"[CH=%d] CLIENT_GET_MESSAGE: SOCKET CLOSED: SELECT FAILED\n", channel);
        return ERR_READ;
    }
    if (retval == 0) {
        //syslog(LOG_ERR,"[CH=%d] TIMEOUT WAITING FOR MESSAGE\n", channel);
        // TIMEOUT
        return COMM_TIMEOUT;
    }

    if (retval > 0) {
        if (FD_ISSET(current_socket, &msg_trigger)) {
	    data_read = udp_read_comm_socket(current_socket, (uint8_t*)saved_buffer, max_buffer_size, NULL, HDRELAY_CHILD_COMM_PORT, daemonport);
            if (data_read < 0) {
                syslog(LOG_ERR,"[CH=%d] CLIENT_GET_MESSAGE: SOCKET CLOSED: READ FAILED\n", channel);
                return ERR_READ;
            }
            if (data_read > 0) {
                uint8_t *p_message_buffer;
                int which_command = ERR_READ;
                
                if (saved_buffer[0] == 0x10 && 
                    saved_buffer[1] == 0x67 && 
                    saved_buffer[2] == REQUEST_TOKEN && 
                    saved_buffer[3] == REQUEST_TOKEN) {
                    which_command = saved_buffer[4];
                    if (which_command > 0 || which_command < 9) {
                        //======== THE COMMAND IS VALID =========
                        //syslog(LOG_INFO,"[CH=%d] CLIENT_GET_MESSAGE: RECEIVEV %d CMD\n", channel, which_command);
                        if (which_command == COMM_CMD_RELOAD && auxdata && auxdata_size) {
                            int temp_size = data_read - 5;
                            if (temp_size > 0 && (temp_size < MAX_UDP_BUFFER - 5)) {
                                for (i = 0; i < temp_size; i++) {
                                    *(auxdata+i) = saved_buffer[5+i];
                                }
                                *auxdata_size = temp_size;
                            }
                            return COMM_CMD_RELOAD;
                        }
                        return which_command;
                    } else {
                        which_command = ERR_READ;
                    }
                }
            }
        }
    }

    //=========== LAST DITCH EFFORT - THE COMMAND WAS NOT PROPERLY PROCESSED - NOT SURE =============
    return ERR_READ;
}

int udp_open_comm_socket(const char *address, int udp_port, int channel)
{
    struct ip_mreq multicastreq;
    struct sockaddr_in recv_addr;
    struct in_addr ip_address;
    int current_socket;
    int reuse_flag = 1;
    int retval; 
    
    retval = inet_aton(address, &ip_address);
    if (!retval) {     
      syslog(LOG_WARNING,"[CH=%d] COMM - invalid address: %s\n", channel, address);
        return -1;
    }

    current_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (current_socket < 0) {        
        syslog(LOG_WARNING,"[CH=%d] COMM - invalid socket address: %s port: %d\n", channel, address, udp_port);
        return -1;
    }

    retval = setsockopt(current_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_flag, sizeof(reuse_flag));
    if (retval < 0) {  
        syslog(LOG_WARNING,"[CH=%d] COMM - address reuse failed - address: %s port: %d\n", channel, address, udp_port);
        close(current_socket);
        return -1;
    }    

    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(udp_port);
    recv_addr.sin_addr.s_addr = ip_address.s_addr; 

    retval = bind(current_socket, (struct sockaddr *)&recv_addr, sizeof(recv_addr));
    if (retval < 0) {     
        syslog(LOG_ERR,"[CH=%d] COMM - unable to bind to address: %s port: %d on eth0\n", channel, address, udp_port);  
        syslog(LOG_ERR,"[CH=%d] COMM - trying alternative interface\n", channel);   
        close(current_socket);
        return -1;
    }  

    syslog(LOG_INFO,"[CH=%d] COMM ADDR: %s PORT: %d SOCKET: %d\n", channel, inet_ntoa(ip_address), udp_port, current_socket);

    return current_socket;
}
