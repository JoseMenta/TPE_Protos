#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include "args.h"

static unsigned short port(const char *s);
static char * path(const char * path);
static void version(void);
static void usage(const char *progname);

static unsigned short
port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "Port should be in the range of 1-65536: %s\n", s);
         exit(1);
     }
     return (unsigned short)sl;
}

static char * path(const char * path){
    unsigned int path_len = strlen(path);
    char * ret_str = calloc(path_len+1,sizeof (char));
    strncpy(ret_str, path, path_len);
    return ret_str;
}

static void user(char *s, char ** user_name, char ** user_pass) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "Password not found for user '%s'\n", s);
        exit(1);
    } else {
        *p = 0;
        p++;
        *user_name = s;
        *user_pass = p;
    }
}

static void
version(void) {
    fprintf(stderr, "POP3 version 0.1\n"
                    "ITBA Protocolos de Comunicación 2023/1 -- Grupo 06\n"
                    "AQUI VA LA LICENCIA\n");
}

static void
usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTION]...\n"
        "\n"
        "   -h               Ayuda.\n"
        "   -p <POP3 port>   Puerto entrante para conexiones POP3.\n"
        "   -P <conf port>   Puerto entrante para conexiones de configuracion del servidor.\n"
        "   -d <path>        Path del directorio Maildir.\n"
        "   -u <name>:<pass> Usuario y contraseña de usuario POP3. Indicarlo para cada usuario que se desea agregar\n"
        "   -v               Imprime información sobre la versión.\n"
        "\n",
        progname);
}

void 
parse_args(const int argc, const char **argv, struct pop3args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->pop3_port = DEFAULT_POP3_PORT;
    args->pop3_config_port = DEFAULT_POP3_CONFIG_PORT;
    args->maildir_path = DEFAULT_MAILDIR_PATH;
    args->users = usersADT_init();

    int c;
    int nusers = 0;

    while (true) {
        c = getopt(argc, (char *const *) argv, "hp:P:u:vd:");
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'h':
                usage(argv[0]);
                exit(0);
            case 'p':
                args->pop3_port = port(optarg);
                break;
            case 'P':
                args->pop3_config_port = port(optarg);
                break;
            case 'u':
                if(nusers >= MAX_USERS) {
                    fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                    exit(1);
                } else {
                    char * user_name, * user_pass;
                    user(optarg, &user_name, &user_pass);
                    usersADT_add(args->users, user_name, user_pass);
                    nusers++;
                }
                break;
            case 'v':
                version();
                exit(0);
            case 'd':
                args->maildir_path = path(optarg);
                break;
            default:
                fprintf(stderr, "Unknown argument: '%c'.\n", c);
                exit(1);
        }
    }
    if (optind < argc) {
        fprintf(stderr, "Argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "'%s' ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
