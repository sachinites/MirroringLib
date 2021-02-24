/*
 * =====================================================================================
 *
 *       Filename:  conn_mgmt.h
 *
 *    Description: This file defined the interfaces for connection mgmt 
 *
 *        Version:  1.0
 *        Created:  02/22/2021 02:31:43 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ABHISHEK SAGAR (), sachinites@gmail.com
 *   Organization:  Juniper Networks
 *
 * =====================================================================================
 */
#ifndef __CONN_MGMT__
#define __CONN_MGMT__

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "../gluethread/glthread.h"

typedef enum {

	COMM_MGMT_CONN_DOWN,
	COMM_MGMT_CONN_UP
} conn_mgmt_conn_status_t;

typedef enum {

	COMM_MGMT_UDP,
	COMM_MGMT_TCP
} conn_mgmt_proto_t;

typedef void *(*conn_mgmt_app_notif_fn_ptr)(
				conn_mgmt_conn_status_t conn_code, 
				char *machine_ip,
				uint32_t port_no,
				void *msg,
				uint32_t msg_size);

#define CONN_MGMT_MAX_CLIENTS_SUPPORTED	8

typedef struct comm_mgmt_conn_thread_ {

	pthread_t conn_thread_handle;
	pthread_mutex_t mutx;
	pthread_cond_t cv;
	void *(*thread_fn)(void *);
	void *thread_fn_arg;
	bool should_pause;
} conn_mgmt_conn_thread_t;

typedef struct conn_mgmt_conn_state_{

	char dest[16];
	conn_mgmt_proto_t proto;
	union {
		uint32_t dst_udp_port;
		uint32_t dst_tcp_port;
	}pon;
	uint16_t keep_alive_interval;
	conn_mgmt_conn_thread_t conn_thread;
	uint32_t ka_recvd;
	uint32_t ka_sent;
	uint32_t down_count;
	conn_mgmt_app_notif_fn_ptr app_notif_cb[CONN_MGMT_MAX_CLIENTS_SUPPORTED];
	glthread_t glue;
} conn_mgmt_conn_state_t;

GLTHREAD_TO_STRUCT(glue_to_conn_state_t, conn_mgmt_conn_state_t, glue);

typedef struct conn_mgmt_conn_db_ {

	glthread_t head;
	uint32_t no_of_connections;
} conn_mgmt_conn_db_t;

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
	char *dest_ip,
	uint32_t port_no,
	conn_mgmt_proto_t proto,
	uint16_t ka_interval,
	conn_mgmt_app_notif_fn_ptr app_notif_cb);

conn_mgmt_conn_state_t *
conn_mgmt_lookup_connection(
	char *dest_ip,
	uint32_t port_no);

bool
conn_mgmt_update_connection(
	char *dest_ip,
	uint32_t port_no,
	conn_mgmt_conn_state_t *new_conn_state);

bool
conn_mgmt_stop_connection(
	char *dest_ip,
	uint32_t port_no);

bool
conn_mgmt_destroy_connection(
	char *dest_ip,
	uint32_t port_no);

bool
conn_mgmt_resume_connection(
	char *dest_ip,
	uint32_t port_no);


#endif /* __CONN_MGMT__  */
