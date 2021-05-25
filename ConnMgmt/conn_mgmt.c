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

    pthread_cond_init(&conn_thread->cv, 0);
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
    pthread_mutex_init(&conn->conn_mutex, NULL);
    conn->pause_sending_kas = false;
    memset(conn->ka_msg.ka_msg, 0, sizeof(conn->ka_msg.ka_msg));
    conn->ka_msg.ka_msg_size = 0;
	return conn;
}

bool
conn_mgmt_pause_sending_kas(
        conn_mgmt_conn_state_t *conn) {

    pthread_mutex_lock(&conn->conn_mutex);
    conn->pause_sending_kas = true;
    pthread_mutex_unlock(&conn->conn_mutex);
}

void
conn_mgmt_resume_sending_kas(
        conn_mgmt_conn_state_t *conn) {

    pthread_mutex_lock(&conn->conn_mutex);
    conn->pause_sending_kas = false;
    pthread_cond_signal(&conn->conn_thread.cv);
    pthread_mutex_unlock(&conn->conn_mutex);
}

void
conn_mgmt_set_conn_ka_interval(
        conn_mgmt_conn_state_t *conn,
        uint32_t ka_interval) {
   
    conn_mgmt_pause_sending_kas(conn);
    pthread_mutex_lock(&conn->conn_mutex);
    conn->keep_alive_interval = ka_interval;
    pthread_mutex_unlock(&conn->conn_mutex);
    conn_mgmt_resume_sending_kas(conn);
}

#define MAX_PACKET_BUFFER_SIZE 256
static unsigned char recv_buffer[MAX_PACKET_BUFFER_SIZE];


static void
pkt_receive( conn_mgmt_conn_state_t *conn,
			 unsigned char *pkt,
			 uint32_t pkt_size) {

    printf("%s() Called ....\n", __FUNCTION__);
}


static void*
conn_mgmt_pkt_recv(void *arg) {

	uint32_t bytes_recvd;
    
    int addr_len = sizeof(struct sockaddr);
    
    conn_mgmt_conn_state_t *conn = (conn_mgmt_conn_state_t *)arg;
    
	struct sockaddr_in sender_addr;
    sender_addr.sin_family      = AF_INET;
    sender_addr.sin_port        = conn->conn_key.src_port_no;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
	
    while(1) {
    
    	memset(recv_buffer, 0, MAX_PACKET_BUFFER_SIZE);
        bytes_recvd = recvfrom(conn->sock_fd, 
							   (char *)recv_buffer, 
                               MAX_PACKET_BUFFER_SIZE, 0,
                               (struct sockaddr *)&sender_addr,
                               &addr_len);
                
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

static int
send_udp_msg(char *dest_ip_addr,
             uint32_t dest_port_no,
             char *msg,
             uint32_t msg_size,
			 int sock_fd) {
    
	struct sockaddr_in dest;

    dest.sin_family = AF_INET;
    dest.sin_port = dest_port_no;
    struct hostent *host = (struct hostent *)gethostbyname(dest_ip_addr);
    dest.sin_addr = *((struct in_addr *)host->h_addr);
    int addr_len = sizeof(struct sockaddr);

    return sendto(sock_fd, msg, msg_size,
            0, (struct sockaddr *)&dest,
            sizeof(struct sockaddr));
}

static int
prepare_ka_pkt (conn_mgmt_conn_state_t *conn,
				unsigned char *ka_pkt,
				uint32_t ka_pkt_size) {

	return CONN_MGMT_KA_PKT_MAX_SIZE;
}

static void *
conn_mgmt_send_ka_pkt(void *arg) {

	conn_mgmt_conn_state_t *conn =
		(conn_mgmt_conn_state_t *)arg;

	conn->ka_msg.ka_msg_size = 
        prepare_ka_pkt(conn, 
                       conn->ka_msg.ka_msg,
                       sizeof(conn->ka_msg.ka_msg));

	while(1) {
	
		send_udp_msg (conn->conn_key.dest_ip,
					  conn->conn_key.dst_port_no,
					  conn->ka_msg.ka_msg,
                      conn->ka_msg.ka_msg_size,
					  conn->sock_fd);
					  
		conn->ka_sent++;
		sleep(conn->keep_alive_interval);
		printf("%s() called ....\n", __FUNCTION__);

        pthread_mutex_lock(&conn->conn_mutex);

        while (conn->pause_sending_kas) {
            pthread_cond_wait(&conn->conn_thread.cv, &conn->conn_mutex);
        }
        pthread_mutex_unlock(&conn->conn_mutex);
	}
	
	return NULL;
}

static void
conn_mgmt_start_ka_sending_thread(
        conn_mgmt_conn_state_t *conn) {

	pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&conn->conn_thread.ka_thread_handle, &attr, 
                    conn_mgmt_send_ka_pkt, (void *)conn);
}


void
conn_mgmt_start_connection(
        conn_mgmt_conn_state_t *conn) {

    assert(conn->keep_alive_interval);
	
	/* Dont start the connection again */
	if (conn->sock_fd > 0) {
		assert(0);
	}
	
    int udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	
	if (udp_sock_fd < 0 ) {
		printf("Socket creation failed, error no = %d\n", udp_sock_fd);
		return;
	}
	
	/*This Socket FD shall be used to send and recv pkts */
    conn->sock_fd = udp_sock_fd;
	
	struct sockaddr_in sender_addr;
    sender_addr.sin_family      = AF_INET;
    sender_addr.sin_port        = conn->conn_key.src_port_no;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(conn->sock_fd,
             (struct sockaddr *)&sender_addr,
             sizeof(struct sockaddr)) == -1) {

        printf("Error : socket bind failed\n");
        return;
    }

    conn->wt = init_wheel_timer(60, 1);
    start_wheel_timer(conn->wt);

	/* Start the thread to recv KA msgs from the other machine */
    conn_mgmt_start_pkt_recvr_thread(conn);
	
	/*Start the thread to send periodic KA messags */
    conn_mgmt_start_ka_sending_thread(conn);
}


int
main(int argc, char **argv) {

	conn_mgmt_conn_state_t *conn;
	conn_mgmt_conn_key_t conn_key;

    if (argc != 5) {

        printf("Usage : ./<executable> <src ip> <src port no> "
                "<dst ip> <dst port no>\n");
        return 0;
    }

	strncpy((char *)&conn_key.src_ip,  argv[1], 16);
	conn_key.src_port_no = atoi(argv[2]);
	strncpy((char *)&conn_key.dest_ip, argv[3], 16);
	conn_key.dst_port_no = atoi(argv[4]);

    /* Create a new connection Object */
	conn = conn_mgmt_create_new_connection(&conn_key);
    /* Set KA interval, if not set, default shall be used */
    conn_mgmt_set_conn_ka_interval(conn, 2);
    /* Start send KA msgs, and get ready to recv msgs */
	conn_mgmt_start_connection(conn);
	
	pthread_exit(0);
    return 0;
}


