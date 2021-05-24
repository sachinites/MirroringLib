/*
 * =====================================================================================
 *
 *       Filename:  conn_mgmt.c
 *
 *    Description: This file implements the routines for connection mgmt 
 *
 *        Version:  1.0
 *        Created:  02/22/2021 01:06:08 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ABHISHEK SAGAR (), sachinites@gmail.com
 *   Organization:  Juniper Networks
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <errno.h>
#include <netdb.h> 
#include <unistd.h>
#include "conn_mgmt.h"

static void
conn_mgmt_init_conn_thread(
	conn_mgmt_conn_thread_t *conn_thread) {

    pthread_mutex_init(&conn_thread->mutex, 0);
    pthread_cond_init(&conn_thread->cv, 0);
    conn_thread->thread_fn = 0;
    conn_thread->thread_fn_arg = 0;
}

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
    conn_mgmt_conn_key_t *conn_key) {

    conn_mgmt_conn_state_t *conn;

	conn = calloc(1, sizeof(conn_mgmt_conn_state_t));	

    memcpy(&conn->conn_key, conn_key, sizeof(conn_mgmt_conn_key_t));
    conn->mastership_state = COMM_MGMT_MASTER;
	conn->keep_alive_interval = CONN_MGMT_DEFAULT_KA_INTERVAL;
	conn_mgmt_init_conn_thread(&conn->conn_thread);
	conn->ka_recvd = 0;
	conn->ka_sent = 0;
	conn->down_count = 0;
	return conn;
}

void
conn_mgmt_set_conn_ka_interval(
        conn_mgmt_conn_state_t *conn,
        uint32_t ka_interval) {
    
    conn->keep_alive_interval = ka_interval;
}

#define MAX_PACKET_BUFFER_SIZE 256
static unsigned char recv_buffer[MAX_PACKET_BUFFER_SIZE];


static void
pkt_receive( conn_mgmt_conn_state_t *conn,
			 unsigned char *pkt,
			 uint32_t pkt_size) {

}


static void*
conn_mgmt_pkt_recv(void *arg) {

	uint32_t bytes_recvd;
    
    int addr_len = sizeof(struct sockaddr);
    
    int udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    
    conn_mgmt_conn_state_t *conn = (conn_mgmt_conn_state_t *)arg;
    
    struct sockaddr_in sender_addr;
    sender_addr.sin_family      = AF_INET;
    sender_addr.sin_port        = conn->conn_key.src_port_no;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(udp_sock_fd, (struct sockaddr *)&sender_addr, sizeof(struct sockaddr)) == -1) {
        printf("Error : socket bind failed\n");
        return 0;
    }
    
    while(1) {
    
    	memset(recv_buffer, 0, MAX_PACKET_BUFFER_SIZE);
        bytes_recvd = recvfrom(udp_sock_fd, (char *)recv_buffer, 
                        MAX_PACKET_BUFFER_SIZE, 0, (struct sockaddr *)&sender_addr, &addr_len);
                
        pkt_receive(conn, recv_buffer, bytes_recvd);
    }    
    
    return 0;
}

static void
conn_mgmt_start_pkt_recvr_thread(conn_mgmt_conn_state_t *conn) {

	pthread_attr_t attr;
    pthread_t recv_pkt_thread;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&recv_pkt_thread, &attr, 
                    conn_mgmt_pkt_recv, (void *)conn);
}


static void *
conn_mgmt_send_ka_pkt(void *arg) {

	
}

static void
conn_mgmt_start_ka_sending_thread(
        conn_mgmt_conn_state_t *conn) {

	pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&conn->ka_thread_handle, &attr, 
                    conn_mgmt_send_ka_pkt, (void *)conn);
}


void
conn_mgmt_start_connection(
        conn_mgmt_conn_state_t *conn) {

    assert(conn->keep_alive_interval);

    conn_mgmt_start_pkt_recvr_thread(conn);
    conn_mgmt_start_ka_sending_thread(conn);
}




