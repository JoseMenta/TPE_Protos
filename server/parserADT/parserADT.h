#ifndef __PARSER_ADT_H__
#define __PARSER_ADT_H__

#include <stdint.h>
#include "../parserDefinition/parserDefinition.h"

typedef struct parserCDT * parserADT;

typedef enum parser_state { PARSER_READING = 0, PARSER_FINISHED, PARSER_ERROR } parser_state;

/**
 * Inicializa el parser POP3
 * 
 * Retorna NULL si no pudo crearse el parser
 */
parserADT parser_init(void);

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
 * en buff, copiando a lo sumo max caracteres incluido \0
 *
 * Retorna NULL si no pudo crear la copia
 */
void get_cmd(parserADT p, char* buff, int max);

/**
 * Devuelve una copia del argumento leido
 * en buff, copiando a lo sumo max caracteres incluido \0
 */
void get_arg(parserADT p, char* buff, int max);

#endif
