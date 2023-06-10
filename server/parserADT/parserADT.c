#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parserADT.h"

#define CMD_LENGTH                  4
#define ARG_MAX_LENGTH              40
#define ASCII_a                     0x61
#define ASCII_z                     0x7A
#define ASCII_PRINTABLE_MIN         0x21
#define ASCII_PRINTABLE_MAX         0x7E
#define ASCII_SPACE                 0x20
#define ASCII_CR                    0x0D
#define ASCII_LF                    0X0A

typedef struct parserCDT {
    const struct parser_definition *    def;                    // Automata
    uint8_t                             state;                  // Current automata state
    bool                                has_error;              // Reached an error state

    char                                cmd[CMD_LENGTH+1];      // Command string
    uint8_t                             cmd_length;             // Command length
    char                                arg[ARG_MAX_LENGTH+1];  // Argument string
    uint8_t                             arg_length;             // Argument length
} parserCDT;


parserADT parser_init(void) {
    parserADT p = malloc(sizeof(parserCDT));
    if(p != NULL) {
        p->def = get_definition();
        parser_reset(p);
    }
    return p;
}

void parser_destroy(parserADT p) {
    if(p != NULL) {
        free(p);
    }
}

void parser_reset(parserADT p) {
    p->state = p->def->start_state;
    memset(p->cmd, 0, CMD_LENGTH+1);
    memset(p->arg, 0, ARG_MAX_LENGTH+1);
    p->has_error=false;
    p->arg_length = p->cmd_length = 0;
}

void get_cmd(parserADT p, char* buff, int max) {
    char cmd [p->cmd_length+1];
    cmd[p->cmd_length] = '\0';
    //hago esto porque puede ser que p->cmd no sea null terminated
    strncpy(cmd, p->cmd, p->cmd_length);
    snprintf(buff,max,"%s",cmd);
}

void get_arg(parserADT p, char* buff, int max) {
    char arg [p->arg_length+1];
    arg[p->arg_length] = '\0';
    //hago esto porque puede ser que p->cmd no sea null terminated
    strncpy(arg, p->arg, p->arg_length);
    //usamos snprintf para copiar siempre el caracter nulo, es decir son max-1 caracteres y \0
    snprintf(buff, max,"%s",arg);
}


parser_state parser_feed(parserADT p, uint8_t c) {
    // Se obtienen las transiciones del estado actual
    const struct parser_state_transition * state = p->def->states[p->state];
    // Se obtiene la cantidad de transiciones del estado actual
    const size_t n                              = p->def->states_n[p->state];
    // Flag para indicar si se encontró una transición que con la condición
    bool matched = false;
    for (size_t i = 0; i < n && !matched; i++) {
        switch (state[i].when) {
            case LETTER:
                c = tolower(c);
                matched = (c >= ASCII_a && c <= ASCII_z);
                break;
            case PRINTABLE:
                matched = (c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX);
                break;
            case SPACE:
                matched = (c == ASCII_SPACE);
                break;
            case CR:
                matched = (c == ASCII_CR);
                break;
            case LF:
                matched = (c == ASCII_LF);
                break;
            case ANY:
                matched = true;
                break;
            default:
                matched = false;
                break;
        }

        if (matched) {
            p->state = state[i].dest;
            switch(state[i].type) {
                case CMD:
                    if(!p->has_error) {
                        p->cmd[p->cmd_length++] = c;
                    }
                    break;
                case ARG:
                    if(!p->has_error) {
                        if (p->arg_length >= 40) {
                            p->has_error=true;
                            break;
                        }
                        p->arg[p->arg_length++] = c;
                    }
                    break;
                case ERR:
                    p->has_error=true;
                    break;
                case END:
                    if(p->has_error){
                        return PARSER_ERROR;
                    }else{
                        return PARSER_FINISHED;
                    }
                default:
                    break;
            }
        }
    }
    return PARSER_READING;
}
