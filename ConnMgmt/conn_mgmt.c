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
#include "conn_mgmt.h"

void
conn_mgmt_init_conn_thread(
	conn_mgmt_conn_thread_t *conn_thread) {


}

conn_mgmt_conn_state_t *
conn_mgmt_create_new_connection(
    char *dest_ip,
    uint32_t port_no,
    conn_mgmt_proto_t proto,
    uint16_t ka_interval,
    conn_mgmt_app_notif_fn_ptr app_notif_cb) {

	int i;

	conn_mgmt_conn_state_t *conn = calloc(1, sizeof(conn_mgmt_conn_state_t));	
	strncpy(conn->dest, dest_ip, 16);
	conn->proto = proto;

	switch(conn->proto) {

		case COMM_MGMT_UDP:
			conn->pon.dst_udp_port = port_no;
			break;
		case COMM_MGMT_TCP:
			conn->pon.dst_tcp_port = port_no;
			break;
		default:
			;
	}
	conn->keep_alive_interval = ka_interval;
	conn_mgmt_init_conn_thread(&conn->conn_thread);
	conn->ka_recvd = 0;
	conn->ka_sent = 0;
	conn->down_count = 0;

	for ( i = 0; i < CONN_MGMT_MAX_CLIENTS_SUPPORTED; i++) {
		if (!conn->app_notif_cb[i]) {
			conn->app_notif_cb[i] = app_notif_cb;
			break;
		}
	}

	if ( i == CONN_MGMT_MAX_CLIENTS_SUPPORTED) {
		free(conn);
		return NULL;
	}

	init_glthread(&conn->glue);
	return conn;
}
