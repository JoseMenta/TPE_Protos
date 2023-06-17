#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <strings.h>   /* strcasecmp */
#include <errno.h>
#include <getopt.h>
#include "args.h"

#define DEFAULT_ACCESS_TOKEN "1234"

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
         fprintf(stderr, "Port should be in the range of 1-65536: '%s'\n", s);
         exit(1);
     }
     return (unsigned short)sl;
}

static unsigned long
max_mails(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)) {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        exit(1);
    }
    return sl;
}

static char * path(const char * path){
    unsigned int path_len = strlen(path);
    char * ret_str = calloc(path_len+1,sizeof (char));
    strncpy(ret_str, path, path_len);
    return ret_str;
}

/*
 * Cambia el directorio maildir del servidor
 * Guarda una copia en el heap, por lo que no se queda con char* maildir
 */
int change_maildir(struct pop3args* args, const char* maildir){
    char* temp = args->maildir_path;
    int maildir_len = strlen(maildir);
    args->maildir_path = calloc(maildir_len+1, sizeof (char));
    if(args->maildir_path == NULL || errno == ENOMEM){
        args->maildir_path = temp; //me quedo con el de antes
        return 1;
    }
    free(temp);
    strncpy(args->maildir_path,maildir,maildir_len);
    return 0;
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

static log_level_t log_level(const char * level) {
    if(strcasecmp(level, "DEBUG") == 0) {
        return LOG_DEBUG;
    } else if(strcasecmp(level, "INFO") == 0) {
        return LOG_INFO;
    } else if(strcasecmp(level, "WARNING") == 0) {
        return LOG_WARNING;
    } else if(strcasecmp(level, "ERROR") == 0) {
        return LOG_ERROR;
    } else if(strcasecmp(level, "FATAL") == 0) {
        return LOG_FATAL;
    } else {
        fprintf(stderr, "Unknown log level: '%s'\n", level);
        exit(1);
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
        "   -l <log level>   Nivel de log. Valores posibles: DEBUG, INFO, WARNING, ERROR, FATAL. Default: INFO.\n"
        "   -v               Imprime información sobre la versión.\n"
        "   -m <max>         La cantidad maxima de mails que lee el servidor de maildir para un usuario\n"
        "   -t <token>       Token utilizado por el cliente para realizar cambios en el servidor\n"
        "\n",
        progname);
}

void 
parse_args(const int argc, const char **argv, struct pop3args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->pop3_port = DEFAULT_POP3_PORT;
    args->pop3_config_port = DEFAULT_POP3_CONFIG_PORT;
    args->maildir_path = NULL;
    args->max_mails = DEFAULT_MAX_MAILS;
    args->users = usersADT_init();
    if(args->users == NULL) {
        exit(1);
    }
    args->log_level = LOG_INFO;
    args->access_token = DEFAULT_ACCESS_TOKEN;

    int c;
    int nusers = 0;

    while (true) {
        c = getopt(argc, (char *const *) argv, "hp:P:u:vd:m:l:t:");
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
            case 'm':
                args->max_mails = max_mails(optarg);
                break;
            case 'l':
                args->log_level = log_level(optarg);
                break;
            case 't':
                args->access_token = optarg;
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
    if(args->maildir_path == NULL){
        fprintf(stderr, "Maildir path missing. Please pass a maildir path with -d <path>");
        exit(1);
    }

}
