#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pop3_parser_definition.h"

#define ASCII_a                     0x61
#define ASCII_z                     0x7A
#define ASCII_PRINTABLE_MIN         0x21
#define ASCII_PRINTABLE_MAX         0x7E
#define ASCII_SPACE                 0x20
#define ASCII_CR                    0x0D
#define ASCII_LF                    0X0A

// Condiciones
static bool is_letter(uint8_t c);
static bool is_printable(uint8_t c);
static bool is_space(uint8_t c);
static bool is_cr(uint8_t c);
static bool is_lf(uint8_t c);
static bool is_any(uint8_t c);

// Acciones
static parser_state cmd_action(void * data, uint8_t c);
static parser_state arg_action(void * data, uint8_t c);
static parser_state err_action(void * data, uint8_t c);
static parser_state end_action(void * data, uint8_t c);
static parser_state def_action(void * data, uint8_t c);

// Inicializacion
static void * pop3_parser_init(void);

// Copia
static void * pop3_parser_copy(void * data);

// Reseteo
static void pop3_parser_reset(void * data);

// Destruccion
static void pop3_parser_destroy(void * data);

enum pop3_states {
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
        { .when = is_letter,           .dest = CMD_1,        .action = cmd_action },
        { .when = is_cr,               .dest = MAYEND,       .action = err_action },
        { .when = is_any,              .dest = ERRST,        .action = err_action },
};
static const struct parser_state_transition ST_CMD_1 [] =  {
        { .when = is_letter,          .dest = CMD_2,        .action = cmd_action },
        { .when = is_cr,              .dest = MAYEND,       .action = err_action },
        { .when = is_any,             .dest = ERRST,        .action = err_action },
};
static const struct parser_state_transition ST_CMD_2 [] =  {
        { .when = is_letter,          .dest = CMD_3,        .action = cmd_action },
        { .when = is_cr,              .dest = MAYEND,       .action = err_action },
        { .when = is_any,             .dest = ERRST,        .action = err_action },
};
static const struct parser_state_transition ST_CMD_3 [] =  {
        { .when = is_letter,          .dest = CMD_4,        .action = cmd_action },
        { .when = is_cr,              .dest = MAYEND,       .action = err_action },
        { .when = is_any,             .dest = ERRST,        .action = err_action },
};
static const struct parser_state_transition ST_CMD_4 [] =  {
        { .when = is_space,           .dest = ARGST,        .action = def_action },
        { .when = is_cr,              .dest = MAYEND,       .action = def_action },
        { .when = is_any,             .dest = ERRST,        .action = err_action },
};
static const struct parser_state_transition ST_ARG [] =  {
        { .when = is_printable,        .dest = ARGST,          .action = arg_action },
        { .when = is_cr,               .dest = MAYEND,         .action = def_action },
        { .when = is_any,              .dest = ERRST,          .action = err_action },
};
static const struct parser_state_transition ST_MAYEND [] =  {
        { .when = is_lf,               .dest = ENDST,          .action = end_action },
        { .when = is_cr,               .dest = MAYEND,         .action = err_action },
        { .when = is_any,              .dest = ERRST,          .action = err_action },
};
static const struct parser_state_transition ST_END [] =  {
        { .when = is_any,              .dest = ENDST,          .action= def_action},
};
static const struct parser_state_transition ST_ERR [] =  {
        { .when = is_cr,               .dest = MAYEND,         .action = err_action },
        { .when = is_any,              .dest = ERRST,          .action = err_action },
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

const parser_definition pop3_parser_definition = {
        .states_count = N(states),
        .states       = states,
        .states_n     = states_n,
        .start_state  = INIT,
        .init         = pop3_parser_init,
        .copy         = pop3_parser_copy,
        .reset        = pop3_parser_reset,
        .destroy      = pop3_parser_destroy
};

char * get_pop3_cmd(pop3_parser_data * d) {
    char * copy = calloc(d->cmd_length + 1, sizeof(char));
    if (copy == NULL) {
        return NULL;
    }
    strncpy(copy, d->cmd, d->cmd_length);
    return copy;
}

char * get_pop3_arg(pop3_parser_data * d) {
    char * copy = calloc(d->arg_length + 1, sizeof(char));
    if (copy == NULL) {
        return NULL;
    }
    strncpy(copy, d->arg, d->arg_length);
    return copy;
}

// Condiciones
static bool is_letter(uint8_t c) {
    c = tolower(c);
    return c >= ASCII_a && c <= ASCII_z;
}
static bool is_printable(uint8_t c) {
    return c >= ASCII_PRINTABLE_MIN && c <= ASCII_PRINTABLE_MAX;
}
static bool is_space(uint8_t c) {
    return c == ASCII_SPACE;
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
static parser_state cmd_action(void * data, uint8_t c) {
    pop3_parser_data * d = (pop3_parser_data *) data;
    if (d->cmd_length < CMD_LENGTH) {
        d->cmd[d->cmd_length++] = c;
        return PARSER_READING;
    }
    return PARSER_ERROR;
}
static parser_state arg_action(void * data, uint8_t c) {
    pop3_parser_data * d = (pop3_parser_data *) data;
    if (d->arg_length < ARG_MAX_LENGTH) {
        d->arg[d->arg_length++] = c;
        return PARSER_READING;
    }
    return PARSER_ERROR;
}
static parser_state err_action(void * data, uint8_t c) {
    return PARSER_ERROR;
}
static parser_state end_action(void * data, uint8_t c) {
    return PARSER_FINISHED;
}
static parser_state def_action(void * data, uint8_t c) {
    return PARSER_READING;
}

// Inicializacion
static void * pop3_parser_init(void) {
    pop3_parser_data * data = calloc(1, sizeof(pop3_parser_data));
    if(data == NULL || errno == ENOMEM) {
        return NULL;
    }
    data->cmd_length = 0;
    data->arg_length = 0;
    return data;
}

// Copia
static void * pop3_parser_copy(void * data) {
    pop3_parser_data * d = (pop3_parser_data *) data;
    pop3_parser_data * copy = calloc(1, sizeof(pop3_parser_data));
    if (copy == NULL || errno == ENOMEM) {
        return NULL;
    }
    copy->cmd_length = d->cmd_length;
    copy->arg_length = d->arg_length;
    strncpy(copy->cmd, d->cmd, d->cmd_length);
    strncpy(copy->arg, d->arg, d->arg_length);
    return copy;
}

// Reseteo
static void pop3_parser_reset(void * data) {
    pop3_parser_data * d = (pop3_parser_data *) data;
    d->cmd_length = 0;
    d->arg_length = 0;
    memset(d->cmd, 0, CMD_LENGTH+1);
    memset(d->arg, 0, ARG_MAX_LENGTH+1);
}

// Destruccion
static void pop3_parser_destroy(void * data) {
    free(data);
}
