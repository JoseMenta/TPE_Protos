#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_LOG_FOLDER "./log"
#define DEFAULT_LOG_FILE (DEFAULT_LOG_FOLDER "/%04d-%02d-%02d_%02d-%02d-%02d.log")
#define DEFAULT_LOG_FILE_MAXSTRLEN 48

/* Tamaño minimo para el buffer */
#define LOG_MIN_BUFFER_SIZE 0x1000 // 4 KBs
/* Tamaño maximo para el buffer */
#define LOG_MAX_BUFFER_SIZE 0x400000 // 4 MBs
/* De a cuanto se agranda el buffer */
#define LOG_BUFFER_SIZE_CHUNK 0x1000 // 4 KBs
/* Tamaño maximo que un unico log deberia requerir */
#define LOG_BUFFER_MAX_PRINT_LENGTH 0x200 // 512 bytes

/* 0666 para darle permisos en la creación del archivo */
#define LOG_FILE_PERMISSION_BITS 0666
/* 0777 para poder entrar al directorio */
#define LOG_FOLDER_PERMISSION_BITS 0777
#define LOG_FILE_OPEN_FLAGS (O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK)

const char* logger_get_level_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:
            return " [DEBUG]";
        case LOG_INFO:
            return " [INFO]";
        case LOG_OUTPUT:
            return " [OUTPUT]";
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


static char* buffer = NULL;
static size_t buffer_start = 0, buffer_length = 0, buffer_capacity = 0;

/* File descriptor para el archivo a escribir */
static int log_file_fd = -1;
static fd_selector selector = NULL;
static log_level_t log_level = MIN_LOG_LEVEL;

/* Stream a escribir logs, por ejemplo stdout */
static FILE* log_stream = NULL;

static void make_buffer_space(size_t len) {
    if (buffer_length + buffer_start + len > buffer_capacity) {
        if (buffer_capacity <= len) {
            memmove(buffer, buffer + buffer_start, buffer_length);
            buffer_start = 0;
        } else if (buffer_capacity < LOG_MAX_BUFFER_SIZE) {
            size_t new_buffer_capacity = buffer_length + len;
            new_buffer_capacity = (new_buffer_capacity + LOG_BUFFER_SIZE_CHUNK - 1) / LOG_BUFFER_SIZE_CHUNK * LOG_BUFFER_SIZE_CHUNK;
            if (new_buffer_capacity > LOG_MAX_BUFFER_SIZE)
                new_buffer_capacity = LOG_MAX_BUFFER_SIZE;

            void* new_buffer = malloc(new_buffer_capacity);
            if (new_buffer == NULL) {
                memmove(buffer, buffer + buffer_start, buffer_length);
                buffer_start = 0;
            } else {
                memcpy(new_buffer, buffer + buffer_start, buffer_length);
                free(buffer);
                buffer = new_buffer;
                buffer_capacity = new_buffer_capacity;
                buffer_start = 0;
            }
        }
    }
}

/*
 Intenta enviar todo lo que puede al archivo de loggeo
 Si quedan bytes a escribir le avisamos al selector
 */
static void try_flush_buffer_to_file() {
    ssize_t written = write(log_file_fd, buffer + buffer_start, buffer_length);
    if (written > 0) {
        buffer_length -= written;
        buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
    }

    // Si quedan para escribir, me sigo interesando en escribir
    selector_set_interest(selector, log_file_fd, buffer_length > 0 ? OP_WRITE : OP_NOOP);
}

static void fd_write_handler(struct selector_key* key) {
    try_flush_buffer_to_file();
}

static void fd_close_handler(struct selector_key* key) {
    // Intento enviar lo que queda

    if (buffer_length != 0) {
        // Lo hago bloqueante para que termine, TODO fijarse si esta bien
        int flags = fcntl(log_file_fd, F_GETFD, 0);
        fcntl(log_file_fd, F_SETFL, flags & (~O_NONBLOCK));
        ssize_t written = write(log_file_fd, buffer, buffer_length);
        if (written > 0) {
            buffer_length -= written;
            buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
        }
    }

    close(log_file_fd);
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

    char log_file_buffer[DEFAULT_LOG_FILE_MAXSTRLEN + 1];

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
    log_file_fd = selector_param == NULL ? -1 : try_open_log_file(log_file, tm);
    log_stream = log_stream_param;
    log_level = MIN_LOG_LEVEL;

    // Registrar el archivo si lo pudimos abrir
    if (log_file_fd >= 0){
        selector_register(selector, log_file_fd, &fdHandler, OP_NOOP, NULL);
    }

    // Chequeo que estoy loggeando por archivo o por Stream
    if (log_file_fd >= 0 || log_stream != NULL) {
        buffer = malloc(LOG_MIN_BUFFER_SIZE);
        buffer_capacity = LOG_MIN_BUFFER_SIZE;
        buffer_length = 0;
        buffer_start = 0;
        if (buffer == NULL) {
            close(log_file_fd);
            log_file_fd = -1;
            fprintf(stderr, "WARNING: Failed to malloc a buffer for logging\n");
            return -1;
        }
    }
    return 0;
}

int logger_finalize() {
    if (log_file_fd >= 0) {
        selector_unregister_fd(selector, log_file_fd);
        selector = NULL;
    }

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
        buffer_capacity = 0;
        buffer_length = 0;
        buffer_start = 0;
    }

    log_stream = NULL;
    return 0;
}

void logger_set_level(log_level_t level) {
    log_level = level;
}

int logger_is_enabled_for(log_level_t level) {
    return level >= log_level && (log_file_fd > 0 || log_stream != NULL);
}

void logger_pre_print() {
    make_buffer_space(LOG_BUFFER_MAX_PRINT_LENGTH);
}

void logger_get_bufstart_and_maxlength(char** bufstartVar, size_t* maxlenVar) {
    *bufstartVar = buffer + buffer_start + buffer_length;
    *maxlenVar = buffer_capacity - buffer_length - buffer_start;
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

    if (log_stream != NULL) {
        fprintf(log_stream, "%s", buffer + buffer_start + buffer_length);
    }

    if (log_file_fd >= 0) {
        buffer_length += written;
        try_flush_buffer_to_file();
    }
    return 0;
}

#endif // #ifndef DISABLE_LOGGER