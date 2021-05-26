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
#include "../libtimer/WheelTimer.h"

typedef enum {

	COMM_MGMT_CONN_DOWN,
    COMM_MGMT_CONN_INIT,
	COMM_MGMT_CONN_UP
} conn_mgmt_conn_status_t;

typedef enum {

    COMM_MGMT_MASTER,
    COMM_MGMT_BACKUP
} conn_mgmt_mastership_state;

#define CONN_MGMT_MAX_CLIENTS_SUPPORTED	8
#define CONN_MGMT_DEFAULT_KA_INTERVAL   5
#define CONN_MGMT_KA_PKT_MAX_SIZE	256

typedef struct conn_mgmt_conn_key_ {

    unsigned char dest_ip[16];
    unsigned char src_ip[16];
    uint32_t src_port_no;
    uint32_t dst_port_no;
} conn_mgmt_conn_key_t;

typedef struct comm_mgmt_conn_thread_ {

	pthread_t ka_thread_handle;
	pthread_cond_t cv;
} conn_mgmt_conn_thread_t;

typedef void *(*conn_mgmt_app_notif_fn_ptr)(
				conn_mgmt_conn_status_t conn_code, 
                conn_mgmt_conn_key_t *conn_key,
				void *msg,
				uint32_t msg_size);

typedef struct ka_msg_ {
    
    unsigned char ka_msg[CONN_MGMT_KA_PKT_MAX_SIZE];
    uint32_t ka_msg_size;
} ka_msg_t;

typedef struct conn_mgmt_conn_state_{

    /* Key of the connection, key is :
     * src ip, src port, dst ip, dst port, proto*/
    conn_mgmt_conn_key_t conn_key;
    /* Master ship state of this machine : Master or backup */
    conn_mgmt_mastership_state mastership_state;
    /* Connection state, whether up or down */
    conn_mgmt_conn_status_t conn_status;
    /* Socket FD created to send and recv msgs */
	int sock_fd;
    /* Time interval in sec to send out KA msgs */
	uint16_t keep_alive_interval;
    /* Connection thread, to send out KA msgs */
	conn_mgmt_conn_thread_t conn_thread;
    /* Some statistics to keep track */
	uint32_t ka_recvd;
	uint32_t ka_sent;
	uint32_t down_count;
    /* Flag to track if sending KA msgs need to be paused */
    bool pause_sending_kas;
    /* KA msg to be sent */
    ka_msg_t ka_msg;
    /* KA msg recvd from peer */
    ka_msg_t peer_ka_msg;
    /* Mutex to update the connection;s properties in a
     * thread safe manner */
    pthread_mutex_t conn_mutex;
    /* List of appln callnacks to be notified */
	conn_mgmt_app_notif_fn_ptr
        app_notif_cb[CONN_MGMT_MAX_CLIENTS_SUPPORTED];
    /* KA Expiry timer */
    wheel_timer_t *wt; /* KA interval + 2 sec */
} conn_mgmt_conn_state_t;

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
    conn_mgmt_conn_key_t *conn_key);

void
conn_mgmt_start_connection(
        conn_mgmt_conn_state_t *conn);

void
conn_mgmt_set_conn_ka_interval(
        conn_mgmt_conn_state_t *conn,
        uint32_t ka_interval);

bool
conn_mgmt_pause_sending_kas(
        conn_mgmt_conn_state_t *conn);

void
conn_mgmt_resume_sending_kas(
        conn_mgmt_conn_state_t *conn);

/* KA pkt format  */

#pragma pack (push,1)

typedef struct ka_pkt_fmt_ {

    unsigned char src_ip_addr[16];
    uint32_t src_port_no;
    unsigned char dst_ip_addr[16];
    uint32_t dst_port_no;
    uint8_t  mastership_state;
    uint8_t conn_state;
    unsigned char my_mac[8];
    unsigned char peer_reported_my_mac[8];
    uint16_t hold_time;
} ka_pkt_fmt_t;

#pragma pack(pop)

#endif /* __CONN_MGMT__  */


