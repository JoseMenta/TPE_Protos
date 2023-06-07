#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>
#include "usersADT.h"

#define DEFAULT_POP3_PORT 1100
#define DEFAULT_POP3_CONFIG_PORT 1101
#define DEFAULT_MAILDIR_PATH "/var/mail"
#define MAX_USERS 500


struct pop3args {
    unsigned short  pop3_port;
    unsigned short  pop3_config_port;
    char *          maildir_path;
    usersADT         users;
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, const char **argv, struct pop3args *args);

#endif

