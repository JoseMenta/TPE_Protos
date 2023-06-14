#include <stdio.h>
#include <stdlib.h>
#include "admin_args.h"

#define VERSION "1"
#define NAME "PROTOS"

void version_init(client_info client);


int main(int argc, const char* argv[]){
    client_info client = malloc(sizeof(struct status_client));
    version_init(client);
    parse_args(argc, argv, client);
    printf("Hello Admin!\n");
    free(client);
}


void version_init( client_info client){
    client->version = VERSION;
    client->name_protocol = NAME;
}
