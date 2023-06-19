#include "admin_args.h"
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void help(const char *progname);

static void showVersion(client_info client, const char * program);

static int user(char *s, char ** user_name, char ** user_pass);

static long number(const char *s);

void parse_args(int argc, const char **argv, client_info client) {
    int c;
    int nusers = 0;
    char buff[DGRAM_SIZE];
    char *user_name, *user_pass;
    char token[50];

    printf("\nIngrese token de verificación:");
    scanf( "%49s", token);

    while (true && client->count_commans < MAX_COMMANDS) {
        c = getopt(argc, (char *const *) argv, "hvA:mM:dD:pcb");

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'h':
                help(argv[0]);
                break;
            case 'v':
                showVersion(client, argv[0]);
                break;
            case 'A':
                if(user(optarg, &user_name, &user_pass)!=0){
                    fprintf(stderr, "Invalid format for user informantion\n");
                    exit(1);
                }
                nusers++;
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n%s\n%s\n\n",
                             client->name_protocol, client->version, token, client->count_commans, client->command_names[ADD_USER], user_name, user_pass);
                client->list_command[client->count_commans].name_command = ADD_USER;
                break;
            case 'm':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n\n",
                         client->name_protocol, client->version, token,client->count_commans, client->command_names[GET_MAX_MAILS]);
                client->list_command[client->count_commans].name_command = GET_MAX_MAILS;
                break;
            case 'M':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n%ld\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[SET_MAX_MAILS], number( optarg));
                client->list_command[client->count_commans].name_command = SET_MAX_MAILS;
                break;
            case 'd':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[GET_MAILDIR]);
                client->list_command[client->count_commans].name_command = GET_MAILDIR;
                break;
            case 'D':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[SET_MAILDIR], optarg);
                client->list_command[client->count_commans].name_command = SET_MAILDIR;
                break;
            case 'p':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[STAT_PREVIOUS_CONNECTIONS]);
                client->list_command[client->count_commans].name_command = STAT_PREVIOUS_CONNECTIONS;
                break;
            case 'c':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[STAT_CURRENT_CONNECTIONS]);
                client->list_command[client->count_commans].name_command = STAT_CURRENT_CONNECTIONS;
                break;
            case 'b':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%s\n%d\n%s\n\n",
                         client->name_protocol, client->version, token, client->count_commans, client->command_names[STAT_BYTES_TRANSFERRED]);
                client->list_command[client->count_commans].name_command = STAT_BYTES_TRANSFERRED;
                break;
            default:
                printf("Invalid state\n");
                exit(1);
        }
        strcpy(client->list_command[client->count_commans].request, buff);
        client->list_command[client->count_commans].timeout=true;
        client->count_commans++;
    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
    }
}


static long number(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0) {
        fprintf(stderr, "number should in in the range of 1-65536: %s\n", s);
        exit(1);
    }
    return sl;
}

static int user(char *s, char ** user_name, char ** user_pass) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found for '%s'\n", s);
        return 1;
    }
    *p = 0;
    p++;
    *user_name = s;
    *user_pass = p;
    return 0;
}

static void showVersion(client_info client, const char * program){
    fprintf(stderr, "%s version: %s -> %s\n", client->name_protocol, client->version, program);
}

static void help(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Ayuda.\n"
            "   -v               Imprime información sobre la versión.\n"
            "   -A <name>:<pass> Agregar un usuario al servidor.\n"
            "   -m               Recibir el numero de mails disponibles maximo.\n"
            "   -M <max>         Modificar el numero de mails disponibles maximo.\n"
            "   -d               Recibir el path al maildir.\n"
            "   -D <path>        Modificar el path al maildir.\n"
            "   -p               Recibir el número de conexiones previas.\n"
            "   -c               Recibir el número de conexiones actuales.\n"
            "   -b               Recibir el número de bytes transferidos.\n"
            "\n",
            progname);
}
