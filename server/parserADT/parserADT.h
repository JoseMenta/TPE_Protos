#ifndef __PARSER_ADT_H__
#define __PARSER_ADT_H__

#include <stdint.h>
#include "../parserDefinition/parserDefinition.h"

typedef struct parserCDT * parserADT;

typedef enum parser_state { READING = 0, FINISHED, ERROR } parser_state;

/**
 * Inicializa el parser POP3
 * 
 * Retorna NULL si no pudo crearse el parser
 */
parserADT parser_init();

/** 
 * Destruye el parser 
 *
 */
void parser_destroy(parserADT p);

/** 
 * permite resetear el parser al estado inicial 
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
 * Devuelve una copia del comando leido
 * Se debe liberar al dejar de utilizarlo
 *
 * Retorna NULL si no pudo crear la copia
 */
const char * get_cmd(parserADT p);

/**
 * Devuelve una copia del argumento leido
 * Se debe liberar al dejar de utilizarlo
 *
 * Retorna NULL si no pudo crear la copia
 */
const char * get_arg(parserADT p);

#endif