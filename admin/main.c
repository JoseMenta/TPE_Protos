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
    if(client == NULL || errno == ENOMEM){
        return 1;
    }
    version_init(client);
    parse_args(argc, argv, client);


    //Address para hacer el bind del socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;             // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // Todas las interfaces (escucha por cualquier IP)
    addr.sin_port        = htons(PORT);         // Server port


    const int server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(server < 0) {
        printf("unable to create socket for ipv4");
        free(client);
        exit(1);
    }

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        printf("unable to bind socket for ipv4");
        free(client);
        exit(1);
    }

    for(int i=0; i < client->count_commans; i++ ){
        //Logica de enviar las cosas
        //printf("%s", client->list_commands[i]);
        //TODO: REVISAR
        if (send(server, (const char*) client->list_commands[i], DGRAM_SIZE, MSG_NOSIGNAL) < 0){
            perror("Error sending request %d to server");
        }
    }

    //char buff[4][DGRAM_SIZE] = {"PROTOS\n1\n3\n+\n123\n\n", "PROTOS\n1\n1\n+\n\n", "PROTOX\n1\n3\n+\nSalida\n\n", "PROTOS\n1\n3\n-\n\n"};
    char buff[DGRAM_SIZE];
    for(int i=0; i<client->count_commans ; i++){
        //Logica de recibir cosas
        recv(server, (char *) buff, DGRAM_SIZE, 0);
        parse_resp(buff, client);
    }

    free(client);
}


void version_init( client_info client){
    client->version = VERSION;
    client->name_protocol = NAME;
    client->command_names = commands_names_mio;
}

