#ifndef __PARSER_ADT_H__
#define __PARSER_ADT_H__

#include <stdint.h>
#include "parser_definition/parser_definition.h"

typedef struct parserCDT {
    const struct parser_definition *    def;                    // Automata
    uint8_t                             state;                  // Current state
    parser_state                        parser_state;           // Current parser state
    void *                              data;                   // Parser data
} parserCDT;

typedef struct parserCDT * parserADT;

/**
 * Inicializa el parser segun la definicion pasada
 *
 * Retorna NULL si no pudo crearse el parser o inicializar la estructura de datos
 */
parserADT parser_init(const parser_definition * def);

/**
 * Destruye el parser y la estructura de datos
 *
 */
void parser_destroy(parserADT p);

/**
 * Resetea el parser al estado inicial y la estructura de datos
 *
 */
void parser_reset(parserADT p);

/**
 * Define el proximo estado del parser en funcion del caracter pasado
 *
 * Devuelve el estado actual del parser
 */
parser_state parser_feed(parserADT p, uint8_t c);

/**
 * Devuelve una copia de la estructura data
 * Devuelve NULL si no se pudo realizar la copia o si no se necesita data
 *
 * Se debe liberar la memoria con free
 */
void * parser_get_data(parserADT p);

#endif

