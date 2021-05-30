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

#include "../CommandParser/cmdtlv.h"
#include "../CommandParser/libcli.h"

#define CMD_CODE_CONFIG_CONNECTION  1
#define CMD_CODE_CONFIG_CONNECTION_PARAMS   2

static int
connection_config_handler(param_t *param,
                          ser_buff_t *tlv_buf,
                          op_mode enable_or_disable) {

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

    param_t *config = libcli_get_config_hook();

    /* config connection <conn_name> <src ip> <src port no> <dst ip> <dst port> <master|backup>*/
    {
        static param_t connection;
        init_param(&connection, CMD, "connection", 0, 0, INVALID, 0, "\"connection\" keyword");
        libcli_register_param(config, &connection);
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
        }
        support_cmd_negation(&connection);
    }
    support_cmd_negation(config);
}

int
main(int argc, char **argv) {

    init_libcli();
    conn_mgmt_build_cli();
    start_shell();
    return 0;
}
