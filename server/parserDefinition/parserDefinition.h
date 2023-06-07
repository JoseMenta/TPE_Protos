
#ifndef __PARSER_DEFINITION_H__
#define __PARSER_DEFINITION_H__

#include <stdint.h>
#include <stddef.h>

typedef enum parser_type { LETTER, ALPHANUMERIC, SPACE, CR, LF, ANY} parser_type;
typedef enum cmd_type { CMD, ARG, END, ERR, OTHER } cmd_type;

/** describe una transición entre estados  */
struct parser_state_transition {
    /* condición: un parser_type */
    parser_type       when;
    /** descriptor del estado destino cuando se cumple la condición */
    unsigned  dest;
    cmd_type type;
};

/** declaración completa de una máquina de estados */
struct parser_definition {
    /** cantidad de estados */
    const unsigned                         states_count;
    /** por cada estado, sus transiciones */
    const struct parser_state_transition **states;
    /** cantidad de estados por transición */
    const size_t                          *states_n;

    /** estado inicial */
    const unsigned                         start_state;
};

const struct parser_definition * get_definition();

#endif
