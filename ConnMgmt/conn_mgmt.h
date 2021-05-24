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

	COMM_MGMT_PROTO_UDP,
	COMM_MGMT_PROTO_TCP
} conn_mgmt_proto_t;

typedef enum {

    COMM_MGMT_MASTER,
    COMM_MGMT_BACKUP
} conn_mgmt_mastership_state;

#define CONN_MGMT_MAX_CLIENTS_SUPPORTED	8
#define CONN_MGMT_DEFAULT_KA_INTERVAL   5

typedef struct comm_mgmt_conn_thread_ {

	pthread_t ka_thread_handle;
	pthread_mutex_t mutex;
	pthread_cond_t cv;
	void *(*thread_fn)(void *);
	void *thread_fn_arg;
} conn_mgmt_conn_thread_t;

typedef struct conn_mgmt_conn_key_ {

    unsigned char dest_ip[16];
    unsigned char src_ip[16];
    uint32_t src_port_no;
    uint32_t dst_port_no;
    conn_mgmt_proto_t proto;
} conn_mgmt_conn_key_t;

typedef void *(*conn_mgmt_app_notif_fn_ptr)(
				conn_mgmt_conn_status_t conn_code, 
                conn_mgmt_conn_key_t *conn_key,
				void *msg,
				uint32_t msg_size);

typedef struct conn_mgmt_conn_state_{

    conn_mgmt_conn_key_t conn_key;
    conn_mgmt_mastership_state mastership_state;
    conn_mgmt_conn_status_t conn_status;
	uint16_t keep_alive_interval;
	conn_mgmt_conn_thread_t conn_thread;
	uint32_t ka_recvd;
	uint32_t ka_sent;
	uint32_t down_count;
	conn_mgmt_app_notif_fn_ptr app_notif_cb[CONN_MGMT_MAX_CLIENTS_SUPPORTED];
} conn_mgmt_conn_state_t;

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
    conn_mgmt_conn_key_t *conn_key);

void
conn_mgmt_set_conn_ka_interval(conn_mgmt_conn_state_t *conn,
                               uint32_t ka_interval);

void
conn_mgmt_start_connection(conn_mgmt_conn_state_t *conn);

#endif /* __CONN_MGMT__  */
