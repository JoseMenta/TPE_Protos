#ifndef __PARSER_ADT_H__
#define __PARSER_ADT_H__

#include <stdint.h>
#include "parser_definition.h"

typedef struct parserCDT * parserADT;

typedef enum parser_state { PARSER_READING = 0, PARSER_ACTION, PARSER_FINISHED, PARSER_ERROR } parser_state;

/**
 * Inicializa el parser segun la definicion pasada
 *
 * Retorna NULL si no pudo crearse el parser o inicializar la estructura de datos
 */
parserADT parser_init(parser_definition def);

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
 */
void * get_data(parserADT p);

#endif

