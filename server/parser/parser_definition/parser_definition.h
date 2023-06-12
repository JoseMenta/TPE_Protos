
#ifndef __PARSER_DEFINITION_H__
#define __PARSER_DEFINITION_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define N(x) (sizeof(x)/sizeof((x)[0]))

typedef enum parser_state { PARSER_READING = 0, PARSER_ACTION, PARSER_FINISHED, PARSER_ERROR } parser_state;

typedef bool (*parser_condition)(uint8_t c);
typedef parser_state (*parser_action)(void * data, uint8_t c);
typedef void * (*parser_definition_init)(void);
typedef void * (*parser_definition_copy)(void * data);
typedef void (*parser_definition_reset)(void * data);
typedef void (*parser_definition_destroy)(void * data);

/** describe una transición entre estados  */
struct parser_state_transition {
    /** condición: un parser_condition */
    parser_condition    when;
    /** descriptor del estado destino cuando se cumple la condición */
    unsigned            dest;
    /** acción a ejecutar cuando se cumple la condición */
    parser_action       action;
};


/** declaración completa de una máquina de estados */
typedef struct parser_definition {
    /** cantidad de estados */
    const unsigned                         states_count;
    /** por cada estado, sus transiciones */
    const struct parser_state_transition **states;
    /** cantidad de estados por transición */
    const size_t                          *states_n;

    /** estado inicial */
    const unsigned                         start_state;

    /** función para inicializar la estructura data */
    /** devuelve NULL si no se pudo inicializar */
    /** si no se necesita data, se puede pasar NULL */
    parser_definition_init init;

    /** función para devolver una copia de la estructura data */
    /** devuelve NULL si no se pudo realizar la copia */
    /** si no se necesita data, se puede pasar NULL */
    parser_definition_copy copy;

    /** función para resetear la estructura data */
    /** si no se necesita data, se puede pasar NULL */
    parser_definition_reset reset;

    /** función para destruir la estructura data */
    /** si no se necesita data, se puede pasar NULL */
    parser_definition_destroy destroy;
} parser_definition;

#endif
