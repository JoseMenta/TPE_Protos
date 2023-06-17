
#include "admin_resp.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SEPARATOR_STRING "\n"

void parse_resp(char * buff, client_info version){
    char *token;
    bool status = true;
    int req_id = 0;
    token = strtok(buff, SEPARATOR_STRING);
    int cmd = 0;
    for(int i=0; token != NULL && i<MAX_LINES_RESP;  i++) {
        switch (i) {
            case 0:
                status = (strcmp(token, version->name_protocol) == 0) && status;
                break;
            case 1:
                status = (strcmp(token, version->version) == 0) && status;
                break;
            case 2:
                req_id = atoi(token);
                cmd = version->list_command[req_id].name_command;
                printf("%s -> ", version->command_names[version->list_command[req_id].name_command]);
                version->list_command[req_id].timeout=false;
                break;
            case 3:
                status = (strcmp(token, OK_TEXT) == 0) && status;
                if (status) {
                    printf("+OK\n");
                } else {
                    printf("-ERR\n");
                    i = MAX_LINES_RESP + 1;
                }
                break;
            case 4:
                if(status && (cmd == GET_MAX_MAILS || cmd == GET_MAILDIR || cmd == STAT_PREVIOUS_CONNECTIONS || cmd == STAT_CURRENT_CONNECTIONS || cmd == STAT_BYTES_TRANSFERRED)){
                    //solo imprimimos si nos manda informacion
                    printf("- %s\n", token);
                }
                break;
        }
        version->list_command[req_id].timeout = false;
        token = strtok(NULL, SEPARATOR_STRING);
    }
}
