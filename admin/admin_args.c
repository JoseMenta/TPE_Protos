#include "admin_args.h"
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_USERS 500


/*
typedef enum{
    ADD_USER, -A
    CHANGE_PASS, -C
    REMOVE_USER, -r
    GET_MAX_MAILS, -m
    SET_MAX_MAILS, -M
    GET_MAILDIR, -d
    SET_MAILDIR, -D
    STAT_PREVIOUS_CONNECTIONS, -p
    STAT_CURRENT_CONNECTIONS, -c
    STAT_BYTES_TRANSFERRED, -b
}admin_command;
*/

void parse_args(const int argc, const char **argv, client_info client) {
    int c;
    int nusers = 0;
    char buff[1024];
    int i = 0;

    while (true) {
        c = getopt(argc, (char *const *) argv, "hvA:C:R:mM:dD:pcbt:");
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
                    char *user_name, *user_pass;
                    user(optarg, &user_name, &user_pass);
                    nusers++;
                    snprintf(buff, 50, "%s\n%s\n%d\n%s\n%s\n%s\n\n",
                                 client->name_protocol, client->version, 2, "token", user_name, user_pass);
                    printf(buff);
                }
                break;
            case 'C':
                printf("en d");
                break;
            case 'R':
                printf("en u");
                break;
            case 'm':
                printf("en p");
                break;
            case 'M':
                printf("en r");
                break;
            case 'd':
                printf("en r");
                break;
            case 'D':
                printf("en r");
                break;
            case 'p':
                printf("en r");
                break;
            case 'c':
                printf("en r");
                break;
            case 'b':
                printf("en r");
                break;
            case 't':
                printf("en t");
                break;
            default:
                printf("en ninguno");
                break;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
    }
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
