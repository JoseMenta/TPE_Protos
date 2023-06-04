#include "parserDefinition.h"

// definición de maquina

enum states {
    INIT,
    CMD_1,
    CMD_2,
    CMD_3,
    CMD_4,
    ARGST,
    MAYEND,
    ENDST,
    ERRST
};

static const struct parser_state_transition ST_INIT [] =  {
        {.when = LETTER,              .dest = CMD_1,        .type=CMD},
        {.when = CR,                  .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_CMD_1 [] =  {
        {.when = LETTER,              .dest = CMD_2,        .type=CMD},
        {.when = CR,                  .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_CMD_2 [] =  {
        {.when = LETTER,              .dest = CMD_3,        .type=CMD},
        {.when = CR,                  .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_CMD_3 [] =  {
        {.when = LETTER,              .dest = CMD_4,        .type=CMD},
        {.when = CR,                  .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_CMD_4 [] =  {
        {.when = CR,                  .dest = MAYEND,       .type=OTHER},
        {.when = SPACE,               .dest = ARGST,          .type=OTHER},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_ARG [] =  {
        {.when = ALPHANUMERIC,        .dest = ARGST,          .type=ARG},
        {.when = CR,                  .dest = MAYEND,       .type=OTHER},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_MAYEND [] =  {
        {.when = LF,                  .dest = ENDST,          .type=END},
        {.when = CR,                  .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};
static const struct parser_state_transition ST_END [] =  {
        {.when = ANY,                 .dest = ENDST,          .type=OTHER},
};
static const struct parser_state_transition ST_ERR [] =  {
        {.when = CR,                 .dest = MAYEND,       .type=ERR},
        {.when = ANY,                 .dest = ERRST,          .type=ERR},
};

static const struct parser_state_transition *states [] = {
        ST_INIT,
        ST_CMD_1,
        ST_CMD_2,
        ST_CMD_3,
        ST_CMD_4,
        ST_ARG,
        ST_MAYEND,
        ST_END,
        ST_ERR
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t states_n [] = {
        N(ST_INIT),
        N(ST_CMD_1),
        N(ST_CMD_2),
        N(ST_CMD_3),
        N(ST_CMD_4),
        N(ST_ARG),
        N(ST_MAYEND),
        N(ST_END),
        N(ST_ERR),
};

const struct parser_definition definition = {
        .states_count = N(states),
        .states       = states,
        .states_n     = states_n,
        .start_state  = INIT,
};

const struct parser_definition * get_definition(){
    return &definition;
}
