
#include "admin_resp.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


void parse_resp(char * buff, client_info version){
    char *token;
    bool status = true;

    token = strtok(buff, "\n");

    for(int i=0; token != NULL && i<MAX_LINES_RESP;  i++) {
        switch (i) {
            case 0:
                status = (strcmp(token, version->name_protocol) == 0) && status;
                break;
            case 1:
                status = (strcmp(token, version->version) == 0) && status;
                break;
            case 2:
                printf("%s -> ", version->command_names[atoi(token)]);
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
                printf("- %s\n", token);
                break;
        }
        token = strtok(NULL, "\n");
    }
}
