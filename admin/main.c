#include <stdio.h>
#include <stdlib.h>
#include "admin_args.h"
#include "admin_resp.h"
#include <netinet/in.h>
#include <string.h>


#define PORT 1024

char * commands_names_mio[STAT_BYTES_TRANSFERRED+1] = {"ADD_USER", "CHANGE_PASS", "REMOVE_USER", "GET_MAX_MAILS", "SET_MAX_MAILS", "GET_MAILDIR", "SET_MAILDIR","STAT_PREVIOUS_CONNECTIONS", "STAT_CURRENT_CONNECTIONS", "STAT_BYTES_TRANSFERRED"};


int main(int argc, const char* argv[]){
    client_info client = malloc(sizeof(struct status_client));
    version_init(client);
    parse_args(argc, argv, client);


    for(int i=0; i < client->count_commans; i++ ){
        //Logica de enviar las cosas
        printf("%s", client->list_commands[i]);
    }

    char buff[4][DGRAM_SIZE] = {"PROTOS\n1\n3\n+\n123\n\n", "PROTOS\n1\n1\n+\n\n", "PROTOX\n1\n3\n+\nSalida\n\n", "PROTOS\n1\n3\n-\n\n"};
    for(int i=0; i<client->count_commans && i < 4; i++){
        //Logica de recibir cosas
        parse_resp(buff[i], client);
    }

    free(client);
}


void version_init( client_info client){
    client->version = VERSION;
    client->name_protocol = NAME;
    client->command_names = commands_names_mio;
}

