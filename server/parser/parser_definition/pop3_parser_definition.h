#ifndef __POP3_PARSER_DEFINITION_H__
#define __POP3_PARSER_DEFINITION_H__

#include <stdint.h>
#include "parser_definition.h"
#include "../parserADT.h"

#define CMD_LENGTH                  4
#define ARG_MAX_LENGTH              248

// Datos
typedef struct pop3_parser_data {
    char cmd[CMD_LENGTH+1];         // Command string
    uint8_t cmd_length;             // Command length
    char arg[ARG_MAX_LENGTH+1];     // Argument string
    uint8_t arg_length;             // Argument length
} pop3_parser_data;


/**
 * Coloca en buff el contenido del comando
 */
void get_pop3_cmd(parserADT p, char* buff, int max);

/**
 * Coloca en buff el contenido del argumento
 */
void get_pop3_arg(parserADT p, char* buff, int max);

#endif
