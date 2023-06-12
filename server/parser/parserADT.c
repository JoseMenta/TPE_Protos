#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "parserADT.h"

typedef struct parserCDT {
    const struct parser_definition *    def;                    // Automata
    uint8_t                             state;                  // Current state
    parser_state                        parser_state;           // Current parser state
    void *                              data;                   // Parser data
} parserCDT;


parserADT parser_init(parser_definition * def) {
    parserADT p = calloc(1, sizeof(parserCDT));
    if(p == NULL || errno == ENOMEM) {
        return NULL;
    }
    if (def->init != NULL) {
        p->data = def->init();
        if(p->data == NULL) {
            free(p);
            return NULL;
        }
    }
    p->def = def;
    p->state = def->start_state;
    p->parser_state = PARSER_READING;
    return p;
}

void parser_destroy(parserADT p) {
    if(p != NULL) {
        if(p->def->destroy != NULL) {
            p->def->destroy(p->data);
        }
        free(p);
    }
}

void parser_reset(parserADT p) {
    p->state = p->def->start_state;
    p->parser_state = PARSER_READING;
    if(p->def->reset != NULL) {
        p->def->reset(p->data);
    }
}

void * get_data(parserADT p) {
    if(p->def->copy == NULL) {
        return NULL;
    }
    return p->def->copy(p->data);
}


parser_state parser_feed(parserADT p, uint8_t c) {
    // Se obtienen las transiciones del estado actual
    const struct parser_state_transition * state = p->def->states[p->state];
    // Se obtiene la cantidad de transiciones del estado actual
    const size_t n                               = p->def->states_n[p->state];
    // Flag para indicar si se encontró una transición que con la condición
    bool matched = false;
    for (size_t i = 0; i < n && !matched; i++) {
        matched = state[i].when(c);

        if (matched) {
            p->state = state[i].dest;
            parser_state resp = state[i].action(p->data, c);
            if (resp == PARSER_FINISHED) {
                return (p->parser_state == PARSER_ERROR) ? p->parser_state : resp;
            }
            else if (resp == PARSER_ERROR) {
                p->parser_state = PARSER_ERROR;
            } else if (resp == PARSER_ACTION) {
                return resp;
            }
        }
    }
    return PARSER_READING;
}
