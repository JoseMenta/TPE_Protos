#include "byte_stuffing_parser_definition.h"

#define ASCII_CR                    0x0D
#define ASCII_LF                    0X0A
#define ASCII_DOT                   0x2E

// Condiciones
static bool is_dot(uint8_t c);
static bool is_cr(uint8_t c);
static bool is_lf(uint8_t c);
static bool is_any(uint8_t c);

// Acciones
static parser_state dot_action(void * data, uint8_t c);
static parser_state def_action(void * data, uint8_t c);


enum byte_stuffing_states {
    LF,
    DOT,
    ANY,
    CR
};

static const struct parser_state_transition ST_LF [] =  {
        { .when = is_dot,           .dest = DOT,        .action = dot_action },
        { .when = is_cr,            .dest = CR,         .action = def_action },
        { .when = is_any,           .dest = ANY,        .action = def_action },
};

static const struct parser_state_transition ST_DOT [] =  {
        { .when = is_cr,            .dest = CR,         .action = def_action },
        { .when = is_any,           .dest = ANY,        .action = def_action },
};

static const struct parser_state_transition ST_ANY [] =  {
        { .when = is_cr,            .dest = CR,         .action = def_action },
        { .when = is_any,           .dest = ANY,        .action = def_action },
};

static const struct parser_state_transition ST_CR [] =  {
        { .when = is_lf,            .dest = LF,         .action = def_action },
        { .when = is_any,           .dest = ANY,        .action = def_action },
};

static const struct parser_state_transition *states [] = {
        ST_LF,
        ST_DOT,
        ST_ANY,
        ST_CR
};

static const size_t states_n [] = {
        N(ST_LF),
        N(ST_DOT),
        N(ST_ANY),
        N(ST_CR)
};

const parser_definition byte_stuffing_parser_definition = {
        .states_count = N(states),
        .states       = states,
        .states_n     = states_n,
        .start_state  = LF,
        .init         = NULL,
        .copy         = NULL,
        .reset        = NULL,
        .destroy      = NULL
};

// Condiciones
static bool is_dot(uint8_t c) {
    return c == ASCII_DOT;
}
static bool is_cr(uint8_t c) {
    return c == ASCII_CR;
}
static bool is_lf(uint8_t c) {
    return c == ASCII_LF;
}
static bool is_any(uint8_t c) {
    return true;
}

// Acciones
static parser_state dot_action(void * data, uint8_t c) {
    return PARSER_ACTION;
}
static parser_state def_action(void * data, uint8_t c) {
    return PARSER_READING;
}
