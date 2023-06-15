#ifndef _LOGGER_H_
#define _LOGGER_H_

// Este logger crea copias de los datos que necesita con los parametros
// El archivo default o con el nombre deseado se encuentra dentro de ./log/

// Definir esto para deshabilitarlo
// #define DISABLE_LOGGER

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h> // Para el struct tm
#include <unistd.h>
#include "../selector.h"

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

#define MIN_LOG_LEVEL LOG_DEBUG
#define MAX_LOG_LEVEL LOG_FATAL

const char* logger_get_level_string(log_level_t level);

#ifdef DISABLE_LOGGER
#define loggerInit(selector, logFile, logStream)
#define loggerFinalize()
#define loggerSetLevel(level)
#define loggerIsEnabledFor(level) 0
#define logf(level, format, ...)
#define log(level, s)
#define logClientAuthenticated(clientId, username, successful)
#else
/*
 Inicializa el logger con "selectorParam" como selector a usar para poder escribir sin bloquearse
 logFile es el archivo en donde se guardan los logs (no funciona todavia utilizar un file default,
 creando un archivo con nombre predeterminado al pasarle "")
 logStreamParam puede ser stdout para también imprimir lo que se loggea a la consola

 */
int logger_init(fd_selector selector_param, const char* log_file, FILE* log_stream_param);

/*
 Termina y envia los logs restantes
 */
int logger_finalize();

/*
 Setea a que nivel se puede loggear (debug, info, error, etc)
 */
void logger_set_level(log_level_t level);

/*
 Se fija si esta permitido loggear a ese nivel
 */
int logger_is_enabled_for(log_level_t level);

/*
 Hace lugar en el buffer para por lo menos una linea mas de logging
 Ojo, tiene tamaño fijo
 */
void logger_pre_print();

void logger_get_bufstart_and_maxlength(char** buf_start_var, size_t* max_len_var);

/*
 Escribe en el logStream
 Actualiza el current del buffer e intenta flushear al archivo de log
 */
int logger_post_print(int written, size_t max_len);

/*
 Macro a utilizar para loggear
 Le pasas el nivel y lo que queres escribir, se fija si podes, hace lugar
 calcula la hora y escribe la hora seguido del log
 */
#define logf(level, format, ...)                                                                                                           \
    if (logger_is_enabled_for(level)) {                                                                                                    \
        logger_pre_print();                                                                                                                \
        time_t loginternal_time = time(NULL);                                                                                              \
        struct tm loginternal_tm = *localtime(&loginternal_time);                                                                          \
        size_t loginternal_maxlen;                                                                                                         \
        char* loginternal_bufstart;                                                                                                        \
        logger_get_bufstart_and_maxlength(&loginternal_bufstart, &loginternal_maxlen);                                                     \
        int loginternal_written = snprintf(loginternal_bufstart, loginternal_maxlen, "[%04d-%02d-%02d %02d:%02d:%02d]%s\t" format "\n",    \
                                           loginternal_tm.tm_year + 1900, loginternal_tm.tm_mday, loginternal_tm.tm_mon + 1,               \
                                           loginternal_tm.tm_hour, loginternal_tm.tm_min, loginternal_tm.tm_sec,                           \
                                           logger_get_level_string(level), ##__VA_ARGS__);                      \
        logger_post_print(loginternal_written, loginternal_maxlen);                                                                          \
    }

// Para loggear sin formato
#define log(level, s) logf(level, "%s", s)

#endif
#endif