#include "admin_args.h"
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_USERS 500

static void help(const char *progname);

static void showVersion(client_info client, const char * program);

static void user(char *s, char ** user_name, char ** user_pass);

static unsigned short number(const char *s);

void parse_args(int argc, const char **argv, client_info client) {
    int c;
    int nusers = 0;
    char buff[DGRAM_SIZE];
    char *user_name, *user_pass;
    char token[50];

    printf("\nIngrese token de verificación:");
    scanf( "%s", token);

    //Otra manera pasarlo al final de la línea de comandos
    //strcpy(token, argv[argc-1]);
    //argc -= 1;


    while (true) {
        c = getopt(argc, (char *const *) argv, "hvA:C:R:mM:dD:pcb");

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
                if (nusers >= MAX_USERS) {
                    fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                    exit(1);
                } else {
                    user(optarg, &user_name, &user_pass);
                    nusers++;
                    snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n%s\n%s\n\n",
                                 client->name_protocol, client->version, client->count_commans, token, client->command_names[ADD_USER], user_name, user_pass);
                    strcpy(client->list_command[client->count_commans].request, buff);
                    client->list_command[client->count_commans].timeout=true;
                    client->list_command[client->count_commans].name_command = ADD_USER;
                    client->count_commans++;
                }
                break;
            case 'C':
                user(optarg, &user_name, &user_pass);
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[CHANGE_PASS], user_name, user_pass);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = CHANGE_PASS;
                client->count_commans++;
                break;
            case 'R':
                user(optarg, &user_name, &user_pass);
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[REMOVE_USER], user_name, user_pass);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = REMOVE_USER;
                client->count_commans++;
                break;
            case 'm':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[GET_MAX_MAILS]);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = GET_MAX_MAILS;
                client->count_commans++;
                break;
            case 'M':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n%d\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[SET_MAX_MAILS], number( optarg));
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = SET_MAX_MAILS;
                client->count_commans++;
                break;
            case 'd':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[GET_MAILDIR]);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = GET_MAILDIR;
                client->count_commans++;
                break;
            case 'D':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[SET_MAILDIR], optarg);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = SET_MAILDIR;
                client->count_commans++;
                break;
            case 'p':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[STAT_PREVIOUS_CONNECTIONS]);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = STAT_PREVIOUS_CONNECTIONS;
                client->count_commans++;
                break;
            case 'c':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[STAT_CURRENT_CONNECTIONS]);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = STAT_CURRENT_CONNECTIONS;
                client->count_commans++;
                break;
            case 'b':
                snprintf(buff, DGRAM_SIZE, "%s\n%s\n%d\n%s\n%s\n\n",
                         client->name_protocol, client->version, client->count_commans, token, client->command_names[STAT_BYTES_TRANSFERRED]);
                strcpy(client->list_command[client->count_commans].request, buff);
                client->list_command[client->count_commans].timeout=true;
                client->list_command[client->count_commans].name_command = STAT_BYTES_TRANSFERRED;
                client->count_commans++;
                break;
            default:
                printf("Invalid state\n");
                break;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s pepe ", argv[optind++]);
        }
        fprintf(stderr, "\n");
    }
}


static unsigned short number(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "number should in in the range of 1-65536: %s\n", s);
        exit(1);
    }
    return (unsigned short)sl;
}

static void user(char *s, char ** user_name, char ** user_pass) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found for '%s'\n", s);
        return;
    } else {
        *p = 0;
        p++;
        *user_name = s;
        *user_pass = p;
    }
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
            "   -C <name>:<pass> Cambiar la contraseña del usuario.\n"
            "   -R <name>:<pass> Remover un usuario del servidor.\n"
            "   -m               Recibir el numero de mails disponibles maximo.\n"
            "   -M <max>         Modificar el numero de mails disponibles maximo.\n"
            "   -d               Recibir el path al maildir.\n"
            "   -D <path>        Modificar el path al maildir.\n"
            "   -p               Recibir el número de conexiones previas.\n"
            "   -c               Recibir el número de conexiones actuales.\n"
            "   -b               Recibir el número de bytes transferidos.\n"
            "   -t <token>       Token de verificacion.\n"
            "\n",
            progname);
}
