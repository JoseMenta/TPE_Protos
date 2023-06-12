#ifndef __POP3_PARSER_DEFINITION_H__
#define __POP3_PARSER_DEFINITION_H__

#include <stdint.h>
#include "parser_definition.h"

#define CMD_LENGTH                  4
#define ARG_MAX_LENGTH              40

// Datos
typedef struct pop3_parser_data {
    char cmd[CMD_LENGTH+1];         // Command string
    uint8_t cmd_length;             // Command length
    char arg[ARG_MAX_LENGTH+1];     // Argument string
    uint8_t arg_length;             // Argument length
} pop3_parser_data;

/**
 * Devuelve una copia del comando POP3
 * Devuelve NULL si no se pudo realizar la copia
 *
 * Se debe liberar la memoria con free
 */
char * get_pop3_cmd(pop3_parser_data * d);

/**
 * Devuelve una copia del argumento POP3
 * Devuelve NULL si no se pudo realizar la copia
 *
 * Se debe liberar la memoria con free
 */
char * get_pop3_arg(pop3_parser_data * d);

#endif
