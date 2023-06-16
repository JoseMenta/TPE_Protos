#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include "../buffer.h"


#define DEFAULT_LOG_FOLDER "./log"
#define DEFAULT_LOG_FILE (DEFAULT_LOG_FOLDER "/%04d-%02d-%02d_%02d-%02d-%02d.log")
#define DEFAULT_LOG_FILE_MAXSTRLEN 48

/* Tamaño minimo para el buffer */
//#define LOGGER_MIN_BUFFER_SIZE 4096 // 4 KBs
/* Tamaño maximo para el buffer */
//#define LOGGER_MAX_BUFFER_SIZE (LOGGER_MIN_BUFFER_SIZE * 1024) // 4 MBs
/* De a cuanto se agranda el buffer */
//#define LOGGER_BUFFER_CHUNK LOGGER_MIN_BUFFER_SIZE // 4 KBs
/* Tamaño maximo que un unico log deberia requerir */
#define LOGGER_BUFFER_MAX_PRINT_LENGTH 512 // 512 bytes

#define LOGGER_BUFFER_SIZE 0x4000 //16 KB como maximo

/* 0666 para darle permisos en la creación del archivo */
#define LOG_FILE_PERMISSION_BITS 0666
/* 0777 para poder entrar al directorio */
#define LOG_FOLDER_PERMISSION_BITS 0777
#define LOG_FILE_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK)

/*
 * Obtiene el string representando el nivel del log
 */
const char* logger_get_level_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:
            return " [DEBUG]";
        case LOG_INFO:
            return " [INFO]";
        case LOG_WARNING:
            return " [WARNING]";
        case LOG_ERROR:
            return " [ERROR]";
        case LOG_FATAL:
            return " [FATAL]";
        default:
            return " [UNKNOWN]";
    }
}

#ifndef DISABLE_LOGGER

/*
 * Usamos un buffer fijo manejado por buffer.c
 * Dejamos variables globales para no tener que pasar siempre un ADT
 * Vamos a intentar compactar manualmente solo cuando no tengamos espacio
 * Si aun no queda espacio luego de compactar, se van a perder logs
 */
//+1 para que siempre este terminado en \0
static uint8_t buff[LOGGER_BUFFER_SIZE+1] = {0};
buffer logger_buffer;
/*
 * buffer_start: desde donde leer en el buffer
 * buffer_length: la longitud de lo almacenado en el buffer (se usa para ver desde donde escribir)
 * buffer_capacity: capacidad disponible del buffer
 */
//static size_t buffer_start = 0, buffer_length = 0, buffer_capacity = 0;

/* File descriptor para el archivo a escribir */
static int log_file_fd = -1;
/* Selector que usa el logger para escribir al archivo sin bloquearse*/
static fd_selector selector = NULL;
static log_level_t log_level = MIN_LOG_LEVEL;

/* Stream a escribir logs, por ejemplo stdout */
static FILE* log_stream = NULL;

static void make_buffer_space(size_t len) {
    //Necesito mas espacio
    size_t max = 0;
    buffer_write_ptr(&logger_buffer,&max);
    //Si me falta espacio, intento compactar
    if(max < len){
        buffer_compact(&logger_buffer);
    }
    buffer_write_ptr(&logger_buffer,&max);
//    if (buffer_length + buffer_start + len > buffer_capacity) {
//        // Para contar lo que esta disponible despues de lo ultimo escrito
//        // buffer_capacity - buffer_length - buffer_start
//        // Si a eso le sumo lo que hay antes de buffer_start
//        // buffer_start
//        // -> buffer_capacity - buffer_length - buffer_start  + buffer_start = buffer_capacity - buffer_length
//        //TODO: no me da esta cuenta, deberia restarle buffer_length
//        if (buffer_capacity <= len) {
//            //puedo solucionarlo moviendo los datos al principio de la memoria
//            memmove(buffer, buffer + buffer_start, buffer_length);
//            buffer_start = 0;
//        } else if (buffer_capacity < LOGGER_MAX_BUFFER_SIZE) {
//            //puedo agrandarlo
//            size_t new_buffer_capacity = buffer_length + len;
//            //Dejo a la capacidad como multiplos de LOGGER_BUFFER_CHUNK
//            new_buffer_capacity = ((new_buffer_capacity + LOGGER_BUFFER_CHUNK - 1) / LOGGER_BUFFER_CHUNK) * LOGGER_BUFFER_CHUNK;
//            //Si me paso del maximo, vuelvo al maximo
//            if (new_buffer_capacity > LOGGER_MAX_BUFFER_SIZE)
//                new_buffer_capacity = LOGGER_MAX_BUFFER_SIZE;
//            //Reservo la memoria para en nuevo buffer
//            void* new_buffer = malloc(new_buffer_capacity);
//            if (new_buffer == NULL) {
//                //No pude reservar memoria, entonces dejo lo maximo posible (que no va a ser len)
//                memmove(buffer, buffer + buffer_start, buffer_length);
//                buffer_start = 0;
//            } else {
//                //copio en contenido (aprovecho y lo hago al principio)
//                memcpy(new_buffer, buffer + buffer_start, buffer_length);
//                free(buffer);
//                buffer = new_buffer;
//                buffer_capacity = new_buffer_capacity;
//                buffer_start = 0;
//            }
//        }
//    }
}

/*
 Intenta enviar lo que puede al archivo
 Si quedan bytes a escribir le avisamos al selector
 */
static void try_flush_buffer_to_file(void) {
    //intenta escribir al archivo de manera no bloqueante (pasamos el flag en open)
    size_t max = 0;
    char* ptr = (char*) buffer_read_ptr(&logger_buffer,&max);
    ssize_t written = write(log_file_fd,  ptr, max);
    buffer_read_adv(&logger_buffer,written);
    //si escribimos algo, avanzamos el buffer
//    if (written > 0) {
//        buffer_length -= written; //dejamos de contar lo que escribimos
//        si lei lo que quedaba en el buffer (buffer_length == 0) me muevo al inicio. Si no, avanzo lo que lei (buffer_start + written)
//        buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
//    }

    bool can_read = buffer_can_read(&logger_buffer);
    // Si quedan para escribir, me suscribo al fd para escritura (OP_WRITE)
    // Si no, no me susctibo (OP_NOOP)
    selector_set_interest(selector, log_file_fd, can_read ? OP_WRITE : OP_NOOP);
}

/*
 * Funcion llamada por el selector cuando puede escribir en el archivo
 */
static void fd_write_handler(struct selector_key* key) {
    try_flush_buffer_to_file();
}

/*
 * Funcion llamada por el selector cuando se hace unregister del fd del archivo
 */
static void fd_close_handler(struct selector_key* key) {
    // Me quedaron cosas por escribir
//    if (buffer_length != 0) {
    if(buffer_can_read(&logger_buffer)){
        int flags = fcntl(log_file_fd, F_GETFD, 0);
        //Lo dejamos de hacer no bloqueante, no nos preocupa quedarnos bloqueados para mandar lo que falta
        fcntl(log_file_fd, F_SETFL, flags & (~O_NONBLOCK));
        size_t max = 0;
        char* ptr = (char*) buffer_read_ptr(&logger_buffer,&max);
        ssize_t written = write(log_file_fd, ptr, max);
        buffer_read_adv(&logger_buffer,written);
//        if (written > 0) {
//            buffer_length -= written;
//            buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
//        }
    }
    //cerramos el fd al archivo
    close(log_file_fd);
    //por si lo vuelven a usar (no va a pasar)
    log_file_fd = -1;
}

static fd_handler fdHandler = {
    .handle_read = NULL,
    .handle_write = fd_write_handler,
    .handle_close = fd_close_handler,
    .handle_block = NULL};


static int try_open_log_file(const char* log_file, struct tm tm) {
    if (log_file == NULL)
        return -1;

    char log_file_buffer[DEFAULT_LOG_FILE_MAXSTRLEN + 1] = {0};

    // Si es "", le ponemos un default (YYYY-MM-DD.log), si ya existe se sigue loggeando ahi
    if (log_file[0] == '\0') {
        snprintf(log_file_buffer, DEFAULT_LOG_FILE_MAXSTRLEN, DEFAULT_LOG_FILE, tm.tm_year + 1900, tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, tm.tm_sec);

        log_file = log_file_buffer;
        struct stat st = {0};

        // Crea la carpeta log si no esta creada
        if(stat(DEFAULT_LOG_FOLDER, &st) == -1) {
            mkdir(DEFAULT_LOG_FOLDER, LOG_FOLDER_PERMISSION_BITS);
        }
    } else {
        // Agrega el directorio al nombre del archivo
        snprintf(log_file_buffer, DEFAULT_LOG_FILE_MAXSTRLEN, "%s/%s", DEFAULT_LOG_FOLDER, log_file);
        log_file = log_file_buffer;
    }

    int fd = open(log_file, LOG_FILE_OPEN_FLAGS, LOG_FILE_PERMISSION_BITS);
    if (fd < 0) {
        fprintf(stderr, "WARNING: Failed to open logging file at '%s'. Logging will be disabled.\n", log_file);
        return -1;
    }

    return fd;
}

int logger_init(fd_selector selector_param, const char* log_file, FILE* log_stream_param) {
    // Fecha actual para crear el default
    time_t timeNow = time(NULL);
    struct tm tm = *localtime(&timeNow);

    selector = selector_param;
    //Si no me pasan el selector, no voy a poder usar un archivo porque no me quiero bloquear
    log_file_fd = selector_param == NULL ? -1 : try_open_log_file(log_file, tm);
    log_stream = log_stream_param;
    log_level = MAX_LOG_LEVEL;

    // Registrar el archivo si lo pudimos abrir
    if (log_file_fd >= 0){
        selector_register(selector, log_file_fd, &fdHandler, OP_NOOP, NULL);
    }

    // Chequeo que estoy loggeando por archivo o por Stream
    if (log_file_fd >= 0 || log_stream != NULL) {
        buffer_init(&logger_buffer,LOGGER_BUFFER_SIZE,buff);
//        buffer = malloc(LOGGER_MIN_BUFFER_SIZE);
//        buffer_capacity = LOGGER_MIN_BUFFER_SIZE; //tamaño total del buffer (memoria reservada)
//        buffer_length = 0;
//        buffer_start = 0;
//        if (buffer == NULL) {
//            close(log_file_fd);
//            log_file_fd = -1;
//            fprintf(stderr, "WARNING: Failed to malloc a buffer for logging\n");
//            return -1;
//        }
    }
    return 0;
}

int logger_finalize(void) {
    //tenemos el archivo, lo sacamos del selector
    if (log_file_fd >= 0) {
        selector_unregister_fd(selector, log_file_fd);
        selector = NULL;
    }

//    if (buffer != NULL) {
//        liberamos el buffer que usa el logger
//        free(buffer);
//        buffer = NULL;
//        buffer_capacity = 0;
//        buffer_length = 0;
//        buffer_start = 0;
//    }

    log_stream = NULL;
    return 0;
}

void logger_set_level(log_level_t level) {
    log_level = level;
}

int logger_is_enabled_for(log_level_t level) {
    return level >= log_level && (log_file_fd > 0 || log_stream != NULL);
}

void logger_pre_print(void) {
    //nos aseguramos de que en el buffer tengamos
    make_buffer_space(LOGGER_BUFFER_MAX_PRINT_LENGTH);
}

void logger_get_bufstart_and_maxlength(char** write_start, size_t* max_len) {
    //desde donde puede escribir
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&logger_buffer,&max);
//    *write_startt = buffer + buffer_start + buffer_length;
    *write_start = (char*) ptr;
    //cuanto puede escribir (si no hay suficiente espacio porque se paso del maximo, se va a perder el log)
//    *max_len = buffer_capacity - buffer_length - buffer_start;
    *max_len = max;
}


int logger_post_print(int written, size_t maxlen) {
    if (written < 0) {
        fprintf(stderr, "Error: snprintf(): '%s'\n", strerror(errno));
        return -1;
    }

    if ((size_t)written >= maxlen) {
        fprintf(stderr, "Error: %lu bytes of logs possibly lost\n", written - maxlen + 1);
        written = maxlen - 1;
    }
    //imprimimos tambien por el stream si lo pasaron
    if (log_stream != NULL) {
        size_t max = 0;
        char* written_ptr = (char*) buffer_write_ptr(&logger_buffer,&maxlen);
        //TODO: revisar
//        fprintf(log_stream, "%s", buffer + buffer_start + buffer_length);
        //El * es para limitar la cantidad de caracteres escritos
        fprintf(log_stream, "%.*s", written,written_ptr);
    }

    if (log_file_fd >= 0) {
        buffer_write_adv(&logger_buffer,written);
//        buffer_length += written; //actualizo buffer_length para reflejar lo que se escribio
        try_flush_buffer_to_file(); //intento mandarlo al archivo
    }
    return 0;
}

#endif // #ifndef DISABLE_LOGGER
