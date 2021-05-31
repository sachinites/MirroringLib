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

static glthread_t connection_db;
static wheel_timer_t *global_timer;

void conn_mgmt_init() {

	init_glthread(&connection_db);
	global_timer = init_wheel_timer(60, 1, TIMER_SECONDS);
	start_wheel_timer(global_timer);
}



static void
conn_mgmt_report_connection_state(
	conn_mgmt_conn_state_t *conn,
	conn_mgmt_conn_status_t conn_status);

static void
conn_mgmt_update_conn_state(
                conn_mgmt_conn_state_t *conn,
                conn_mgmt_conn_status_t new_state);
	
	
static void
conn_mgmt_init_conn_thread(
	conn_mgmt_conn_thread_t *conn_thread) {

    pthread_cond_init(&conn_thread->cv, 0);
}

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
    conn_mgmt_conn_key_t *conn_key, unsigned char *mastership) {

    conn_mgmt_conn_state_t *conn;

	conn = calloc(1, sizeof(conn_mgmt_conn_state_t));	
    memcpy(&conn->conn_key, conn_key, sizeof(conn_mgmt_conn_key_t));
    conn->mastership_state = strncmp(mastership, "master", strlen("master")) == 0 ?
                             COMM_MGMT_MASTER :
                             COMM_MGMT_BACKUP;
	conn->keep_alive_interval = CONN_MGMT_DEFAULT_KA_INTERVAL;
	conn_mgmt_init_conn_thread(&conn->conn_thread);
	conn->ka_recvd = 0;
	conn->ka_sent = 0;
	conn->down_count = 0;
    pthread_mutex_init(&conn->conn_mutex, NULL);
    conn->pause_sending_kas = false;
    memset(conn->ka_msg.ka_msg, 0, sizeof(conn->ka_msg.ka_msg));
    conn->ka_msg.ka_msg_size = 0;
    memset(conn->peer_ka_msg.ka_msg, 0, sizeof(conn->peer_ka_msg.ka_msg));
    conn->peer_ka_msg.ka_msg_size = 0;
    init_glthread(&conn->glue);
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
    conn->hold_time = conn->keep_alive_interval * 2;
    pthread_mutex_unlock(&conn->conn_mutex);
    conn_mgmt_resume_sending_kas(conn);
}

static int
conn_mgmt_update_ka_pkt (conn_mgmt_conn_state_t *conn,
				unsigned char *ka_pkt,
				uint32_t ka_pkt_size) {

    assert(ka_pkt_size >= sizeof(ka_pkt_fmt_t));

    ka_pkt_fmt_t *ka_pkt_fmt = (ka_pkt_fmt_t *)ka_pkt;

    strncpy(ka_pkt_fmt->src_ip_addr, conn->conn_key.src_ip,
            sizeof(conn->conn_key.src_ip));
    ka_pkt_fmt->src_port_no = conn->conn_key.src_port_no;
    strncpy(ka_pkt_fmt->dst_ip_addr, conn->conn_key.dest_ip,
            sizeof(conn->conn_key.dest_ip));
    ka_pkt_fmt->dst_port_no = conn->conn_key.dst_port_no;
    ka_pkt_fmt->mastership_state = conn->mastership_state;
    ka_pkt_fmt->conn_state = conn->conn_status;
    memset(ka_pkt_fmt->my_mac, 0xff, sizeof(ka_pkt_fmt->my_mac));
    memset(ka_pkt_fmt->peer_reported_my_mac, 0xff, 
           sizeof(ka_pkt_fmt->peer_reported_my_mac));
    ka_pkt_fmt->hold_time = conn->keep_alive_interval * 2;
    return sizeof(ka_pkt_fmt_t);
}

static void
ka_pkt_print(ka_pkt_fmt_t *ka_pkt_fmt) {

    printf("\t\tSrc ip and Port No : %s %u\n",
            ka_pkt_fmt->src_ip_addr, ka_pkt_fmt->src_port_no);
    printf("\t\tdst ip and Port No : %s %u\n",
            ka_pkt_fmt->dst_ip_addr, ka_pkt_fmt->dst_port_no);
    
    switch(ka_pkt_fmt->mastership_state) {
        case COMM_MGMT_MASTER:
            printf("\t\tmastership state : Master\n");
            break;
        case COMM_MGMT_BACKUP:
            printf("\t\tmastership state : Backup\n");
            break;
        default: ;
    }
    switch(ka_pkt_fmt->conn_state){
        case COMM_MGMT_CONN_DOWN:
            printf("\t\tConn State : Down\n");
            break;
        case COMM_MGMT_CONN_INIT:
            printf("\t\tConn State : Init\n");
            break;
        case COMM_MGMT_CONN_UP:
            printf("\t\tConn State : Up\n");
            break;
        default : ;
    }
    printf("\t\tmy mac : %s\n", ka_pkt_fmt->my_mac);
    printf("\t\tpeer reported my mac : %s\n",
            ka_pkt_fmt->peer_reported_my_mac);
    printf("\t\thold time : %u\n", ka_pkt_fmt->hold_time);
}


#define MAX_PACKET_BUFFER_SIZE 256
static unsigned char recv_buffer[MAX_PACKET_BUFFER_SIZE];

static void
conn_mgmt_report_connection_status_to_clients(
	conn_mgmt_conn_state_t *conn) {

    
}

static void
conn_mgmt_report_pre_switchover_to_clients(
        conn_mgmt_conn_state_t *conn) {

    

}

static void
conn_mgmt_report_post_switchover_to_clients(
        conn_mgmt_conn_state_t *conn) {

    
}

static void
conn_mgmt_switchover(conn_mgmt_conn_state_t *conn) {


    if (conn->mastership_state == COMM_MGMT_BACKUP){
        
        conn_mgmt_report_pre_switchover_to_clients(conn);

        conn->mastership_state = COMM_MGMT_MASTER;
    
        conn_mgmt_update_ka_pkt(conn, 
                       conn->ka_msg.ka_msg,
                       sizeof(conn->ka_msg.ka_msg));
        conn_mgmt_report_post_switchover_to_clients(conn);
    }
}

static void
conn_mgmt_tear_conn_down (void *arg, unsigned int arg_size) {

	conn_mgmt_conn_state_t *conn = (conn_mgmt_conn_state_t *)arg;
    timer_de_register_app_event(conn->conn_hold_timer);
    conn->conn_hold_timer = NULL;
    conn_mgmt_update_conn_state(conn,
            COMM_MGMT_CONN_DOWN);
    conn_mgmt_update_ka_pkt(conn, 
                       conn->ka_msg.ka_msg,
                       sizeof(conn->ka_msg.ka_msg));
    conn_mgmt_report_connection_status_to_clients(conn);
    conn_mgmt_switchover(conn);
}

static void
conn_mgmt_refresh_conn_expiration_timer(
	conn_mgmt_conn_state_t *conn) {

	if (!conn->conn_hold_timer) {
		conn->conn_hold_timer = timer_register_app_event(
                                conn->wt,
								conn_mgmt_tear_conn_down,
								(void *)conn, sizeof(conn),
								conn->hold_time * 1000,
								0); 
		return;				
	}
	
	wt_elem_reschedule(conn->conn_hold_timer,
                       conn->hold_time * 1000);
}

static void
conn_mgmt_update_conn_state(
        conn_mgmt_conn_state_t *conn,
        conn_mgmt_conn_status_t new_state) {

	bool conn_state_changed = false;
	
	switch(conn->conn_status) {
	
		case COMM_MGMT_CONN_DOWN:
			conn->conn_status = COMM_MGMT_CONN_INIT;
			conn_state_changed = true;
			break;
			
    	case COMM_MGMT_CONN_INIT:
    		conn->conn_status = COMM_MGMT_CONN_UP;
    		conn_mgmt_refresh_conn_expiration_timer(conn);
    		conn_state_changed = true;
    		break;
    		
		case COMM_MGMT_CONN_UP:
            conn->conn_status = new_state;
			conn_mgmt_refresh_conn_expiration_timer(conn);
			break;
		default: ;
	}
	
	if (conn_state_changed) {
        conn_mgmt_update_ka_pkt(conn, 
                       conn->ka_msg.ka_msg,
                       sizeof(conn->ka_msg.ka_msg));
		conn_mgmt_report_connection_status_to_clients(conn);
	}
}

static void
pkt_receive( conn_mgmt_conn_state_t *conn,
			 unsigned char *pkt,
			 uint32_t pkt_size) {
  
    assert(pkt_size <= CONN_MGMT_KA_PKT_MAX_SIZE);
    conn->ka_recvd++;
    
    if (conn->peer_ka_msg.ka_msg_size != pkt_size ||
    	 memcmp(conn->peer_ka_msg.ka_msg, pkt, pkt_size)) {
    	
    	memcpy(conn->peer_ka_msg.ka_msg, pkt, pkt_size);
    	conn_mgmt_update_conn_state(conn,
                conn_mgmt_get_next_conn_state(conn->conn_status));
    }
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


static void *
conn_mgmt_send_ka_pkt(void *arg) {

	conn_mgmt_conn_state_t *conn =
		(conn_mgmt_conn_state_t *)arg;

	conn->ka_msg.ka_msg_size = 
        conn_mgmt_update_ka_pkt(conn, 
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

    conn->wt = global_timer;

	/* Start the thread to recv KA msgs from the other machine */
    conn_mgmt_start_pkt_recvr_thread(conn);
	
	/*Start the thread to send periodic KA messags */
    conn_mgmt_start_ka_sending_thread(conn);
}

static void
conn_mgmt_report_connection_state(
	conn_mgmt_conn_state_t *conn,
	conn_mgmt_conn_status_t conn_status) {

	printf("connection status reported : %u\n", conn_status);
}

/* mgmt APIs */

conn_mgmt_conn_state_t *
conn_mgmt_lookup_connection_by_name(char *conn_name) {
	
	return NULL;
}

conn_mgmt_conn_state_t *
conn_mgmt_lookup_connection_by_key(conn_mgmt_conn_key_t *conn_key) {
	
	return NULL;
}

/* backend handlers */
void
conn_mgmt_configure_connection(char *conn_name,
							   char *src_ip,
							   uint16_t src_port_no,
    						   char *dst_ip,
    						   uint16_t dst_port_no,
    						   char *mastership) {
    						   
   conn_mgmt_conn_state_t *conn;
   conn_mgmt_conn_key_t conn_key;
   
   conn = conn_mgmt_lookup_connection_by_name(conn_name);
   
   if (conn) {
 		printf("connection %s already exists\n", conn_name);
 		return;  
   }
   
   strncpy((char *)&conn_key.src_ip,  src_ip, 16);
   conn_key.src_port_no = src_port_no;
   strncpy((char *)&conn_key.dest_ip, dst_ip, 16);
   conn_key.dst_port_no = dst_port_no;
 
   conn = conn_mgmt_lookup_connection_by_key(&conn_key);
   
   if (conn) {
 		printf("connection %s with these keys already exists\n", conn->conn_name);
 		return;    
   }
   
   /* Create a new connection Object */
	conn = conn_mgmt_create_new_connection(&conn_key, mastership);
	strncpy(conn->conn_name, conn_name, sizeof(conn->conn_name));
    /* Set KA interval, if not set, default shall be used */
    conn_mgmt_set_conn_ka_interval(conn, 2);
    /* Start send KA msgs, and get ready to recv msgs */
	conn_mgmt_start_connection(conn);

	glthread_add_next(&connection_db, &conn->glue);
}

static void
conn_mgmt_print_connection_details(conn_mgmt_conn_state_t *conn) {

	printf("conn name : %s\n", conn->conn_name);
	printf("\tconn key : src : %s %u\n", conn->conn_key.src_ip, conn->conn_key.src_port_no);
	printf("\tconn key : dst : %s %u\n", conn->conn_key.dest_ip, conn->conn_key.dst_port_no);
	printf("\tmastership status : %s\n", conn->mastership_state == COMM_MGMT_MASTER ? "master" : "backup");
	switch(conn->conn_status) {
	
		case COMM_MGMT_CONN_DOWN:
			printf("\tconnection state : Down\n");
			break;
    	case COMM_MGMT_CONN_INIT:
    		printf("\tconnection state : Init\n");
			break;
		case COMM_MGMT_CONN_UP:
		printf("\tconnection state : UP\n");
			break;
		default : ;
	}
	
	printf("\tKA Interval : %u  hold time : %u\n",
		conn->keep_alive_interval, conn->hold_time);
		
	printf("\tKA recvd :%u   KA sent :%u   Down Count :%u\n",
		conn->ka_recvd, conn->ka_sent, conn->down_count);
		
	printf("\tKA sending paused : %s\n", conn->pause_sending_kas ? "true" : "false");
	printf("\thold time remaining : %u msec\n", 
		conn->conn_hold_timer ? wt_get_remaining_time(conn->conn_hold_timer) :
        0);
		
	printf("\t Local KA msg : \n");
	ka_pkt_print(conn->ka_msg.ka_msg);
	
	printf("\n\t Peer KA msg \n");
	ka_pkt_print(conn->peer_ka_msg.ka_msg);
		
	printf("*** connection details end *****\n");
	
}



void
conn_mgmt_show_connections(char *conn_name) {


	glthread_t *curr;
	conn_mgmt_conn_state_t *conn;
	
	ITERATE_GLTHREAD_BEGIN(&connection_db, curr) {
	
		conn = glthread_glue_to_connection(curr);
		
		if (conn_name ) {
		
			if (strncmp(conn_name, conn->conn_name, sizeof(conn->conn_name)) == 0) {
				conn_mgmt_print_connection_details(conn);
				return;
			}
			else continue;
		}
		
		conn_mgmt_print_connection_details(conn);
		
	} ITERATE_GLTHREAD_END(&connection_db, curr);
}




