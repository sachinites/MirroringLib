/*
 * =====================================================================================
 *
 *       Filename:  conn_mgmt_ui.c
 *
 *    Description: This file implements the functionality to provide CLI User interface  to Connextion Mgmt Module
 *
 *        Version:  1.0
 *        Created:  05/30/2021 08:04:03 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ABHISHEK SAGAR (), sachinites@gmail.com
 *   Organization:  Juniper Networks
 *
 * =====================================================================================
 */

#include <stdint.h>
#include "conn_mgmt.h"
#include "../CommandParser/cmdtlv.h"
#include "../CommandParser/libcli.h"

#define CMD_CODE_CONFIG_CONNECTION  		1
#define CMD_CODE_CONFIG_CONNECTION_PARAMS   2
#define CMD_CODE_SWITCHOVER					3
#define CMD_CODE_SHOW_CONNECTIONS 			4
#define CMD_CODE_CONFIG_CONNECTION_KA_STOP	5
#define CMD_CODE_CONFIG_CONNECTION_KA_INTERVAL	6

   							
static int
connection_config_handler(param_t *param,
                          ser_buff_t *tlv_buf,
                          op_mode enable_or_disable) {

	char *conn_name = NULL;
	char *src_ip;
	char *dst_ip;
	uint16_t src_port_no;
	uint16_t dst_port_no;
	char *mastership = NULL;
	int cmd_code;
	tlv_struct_t *tlv = NULL;
	
	cmd_code = EXTRACT_CMD_CODE(tlv_buf);
	
	TLV_LOOP_BEGIN(tlv_buf, tlv){

    	if(strncmp(tlv->leaf_id, "conn-name", strlen("conn-name")) ==0)
            conn_name = tlv->value;
        else if (strncmp(tlv->leaf_id, "src-ip", strlen("src-ip")) ==0)
			src_ip = tlv->value;
		else if (strncmp(tlv->leaf_id, "dst-ip", strlen("dst-ip")) ==0)
			dst_ip = tlv->value;
		else if (strncmp(tlv->leaf_id, "src-port-no", strlen("src-port-no")) ==0)
			src_port_no = atoi(tlv->value);
		else if (strncmp(tlv->leaf_id, "dst-port-no", strlen("dst-port-no")) ==0)
			dst_port_no = atoi(tlv->value);
		else if (strncmp(tlv->leaf_id, "mastership", strlen("mastership")) ==0)
			mastership = tlv->value;
		else
			assert(0);
		
    }TLV_LOOP_END;
    
    switch(cmd_code) {
    	case CMD_CODE_CONFIG_CONNECTION:
    	switch(enable_or_disable) {
    		case CONFIG_ENABLE:
    			break;
    		case CONFIG_DISABLE:
    			conn_mgmt_ui_destory_connection(conn_name);
    			break;
    		default:
    		;
    	}
    	break;
    	case CMD_CODE_CONFIG_CONNECTION_PARAMS:
    	conn_mgmt_configure_connection(conn_name, src_ip, src_port_no,
    								dst_ip, dst_port_no, mastership);
    	break;
    	case CMD_CODE_CONFIG_CONNECTION_KA_STOP:
    	break;
    	case CMD_CODE_CONFIG_CONNECTION_KA_INTERVAL:
    	break;
    	default:
    	;
    }			
    return 0;
}

static int
switchover_handler(param_t *param,
                   ser_buff_t *tlv_buf,
                   op_mode enable_or_disable) {

    return 0;
}

static int
show_connections_handler(param_t *param,
                   		 ser_buff_t *tlv_buf,
                   		 op_mode enable_or_disable) {
                   		 
    char *conn_name = NULL;
	tlv_struct_t *tlv = NULL;
	
	TLV_LOOP_BEGIN(tlv_buf, tlv){
	
		conn_name = tlv->value;
		
	}TLV_LOOP_END;

	conn_mgmt_show_connections(conn_name);
		
    return 0;
}

static int
validate_mastership_string(char *value) {

    if (strncmp(value, "master", strlen("master")) == 0 ||
         strncmp(value, "backup", strlen("backup")) == 0) {

        return VALIDATION_SUCCESS;
    }

    return VALIDATION_FAILED;
}

static void
conn_mgmt_build_cli() {

    param_t *config_hook = libcli_get_config_hook();
    param_t *run_hook = libcli_get_run_hook();
    param_t *show_hook = libcli_get_show_hook();

    /* config connection <conn_name> <src ip> <src port no> <dst ip> <dst port> <master|backup>*/
    {
        static param_t connection;
        init_param(&connection, CMD, "connection", 0, 0, INVALID, 0, "\"connection\" keyword");
        libcli_register_param(config_hook, &connection);
        {
            static param_t conn_name;
            init_param(&conn_name, LEAF, 0, connection_config_handler, 0, STRING, "conn-name", "Connection Name");
            libcli_register_param(&connection, &conn_name);
            set_param_cmd_code(&conn_name, CMD_CODE_CONFIG_CONNECTION);
            {
                static param_t src_ip;
                init_param(&src_ip, LEAF, 0, 0, 0, IPV4, "src-ip", "Source IP Address");
                libcli_register_param(&conn_name, &src_ip);
                {
                    static param_t src_port_no;
                    init_param(&src_port_no, LEAF, 0, 0, 0, INT, "src-port-no", "Source Port NO");
                    libcli_register_param(&src_ip, &src_port_no);
                    {
                        static param_t dst_ip;
                        init_param(&dst_ip, LEAF, 0, 0, 0, IPV4, "dst-ip", "Destination IP Address");
                        libcli_register_param(&src_port_no, &dst_ip);
                        {
                            static param_t dst_port_no;
                            init_param(&dst_port_no, LEAF, 0, 0, 0, INT, "dst-port-no", "Destination Port NO");
                            libcli_register_param(&dst_ip, &dst_port_no);
                            {
                                static param_t mastership;
                                init_param(&mastership, LEAF, 0, connection_config_handler, validate_mastership_string, STRING, "mastership", "Mastership : master | backup");
                                libcli_register_param(&dst_port_no, &mastership);
                                set_param_cmd_code(&mastership, CMD_CODE_CONFIG_CONNECTION_PARAMS);
                            }
                        }
                    }
                }
            }
            {
            	/* config connection <conn-name> keep-alive ... */
            	static param_t keep_alive;
            	init_param(&keep_alive, CMD, "keep-alive", 0, 0, INVALID, 0, "Keep Alive Options");
            	libcli_register_param(&conn_name, &keep_alive);
            	{
	            	/* config connection <conn-name> keep-alive stop */
            		static param_t ka_stop;
	            	init_param(&ka_stop, CMD, "stop", connection_config_handler, 0, INVALID, 0, "Stop");
    	        	libcli_register_param(&keep_alive, &ka_stop);
    	        	set_param_cmd_code(&ka_stop, CMD_CODE_CONFIG_CONNECTION_KA_STOP);
            	}
            	{
	            	/* config connection <conn-name> keep-alive interval */
            		static param_t ka_interval;
	            	init_param(&ka_interval, CMD, "interval", 0, 0, INVALID, 0, "KA interval in sec");
    	        	libcli_register_param(&keep_alive, &ka_interval);
    	        	/* config connection <conn-name> keep-alive interval <interval-value in sec>*/
    	        	{
    	        		static param_t interval_value;
                        init_param(&interval_value, LEAF, 0, connection_config_handler, 0, INT, "interval-val", "KA interval value in sec");
                        libcli_register_param(&ka_interval, &interval_value);
                        set_param_cmd_code(&interval_value, CMD_CODE_CONFIG_CONNECTION_KA_INTERVAL);
    	        	}
            	}
            }
        }
        support_cmd_negation(&connection);
    }
    support_cmd_negation(config_hook);

    {
        /* run switchover */
        static param_t switchover;
        init_param(&switchover, CMD, "switchover", switchover_handler, 0, INVALID, 0, "\"switchover\" keyword");
        libcli_register_param(run_hook, &switchover);
        set_param_cmd_code(&switchover, CMD_CODE_SWITCHOVER);
    }
    
    {
    	/* show connections */
    	static param_t connections;
    	init_param(&connections, CMD, "connections", show_connections_handler, 0, INVALID, 0, "\"connections\" keyword");
        libcli_register_param(show_hook, &connections);
        set_param_cmd_code(&connections, CMD_CODE_SHOW_CONNECTIONS);
        {
	        /* show connections <conn-name>*/
        	static param_t conn_name;
            init_param(&conn_name, LEAF, 0, show_connections_handler, 0, STRING, "conn-name", "Connection Name");
            libcli_register_param(&connections, &conn_name);
            set_param_cmd_code(&conn_name, CMD_CODE_SHOW_CONNECTIONS);
        }
    }
}

extern void conn_mgmt_init();

int
main(int argc, char **argv) {

    init_libcli();
    conn_mgmt_build_cli();
    conn_mgmt_init();
    start_shell();
    return 0;
}
