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

const char* loggerGetLevelString(TLogLevel level) {
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
static size_t bufferStart = 0, bufferLength = 0, bufferCapacity = 0;

/* File descriptor para el archivo a escribir */
static int logFileFd = -1;
static fd_selector selector = NULL;
static TLogLevel logLevel = MIN_LOG_LEVEL;

/* Stream a escribir logs, por ejemplo stdout */
static FILE* logStream = NULL;

static void makeBufferSpace(size_t len) {
    if (bufferLength + bufferStart + len > bufferCapacity) {
        if (bufferCapacity <= len) {
            memmove(buffer, buffer + bufferStart, bufferLength);
            bufferStart = 0;
        } else if (bufferCapacity < LOG_MAX_BUFFER_SIZE) {
            size_t newBufferCapacity = bufferLength + len;
            newBufferCapacity = (newBufferCapacity + LOG_BUFFER_SIZE_CHUNK - 1) / LOG_BUFFER_SIZE_CHUNK * LOG_BUFFER_SIZE_CHUNK;
            if (newBufferCapacity > LOG_MAX_BUFFER_SIZE)
                newBufferCapacity = LOG_MAX_BUFFER_SIZE;

            void* newBuffer = malloc(newBufferCapacity);
            if (newBuffer == NULL) {
                memmove(buffer, buffer + bufferStart, bufferLength);
                bufferStart = 0;
            } else {
                memcpy(newBuffer, buffer + bufferStart, bufferLength);
                free(buffer);
                buffer = newBuffer;
                bufferCapacity = newBufferCapacity;
                bufferStart = 0;
            }
        }
    }
}

/*
 Intenta enviar todo lo que puede al archivo de loggeo
 Si quedan bytes a escribir le avisamos al selector
 */
static void tryFlushBufferToFile() {
    ssize_t written = write(logFileFd, buffer + bufferStart, bufferLength);
    if (written > 0) {
        bufferLength -= written;
        bufferStart = (bufferLength == 0 ? 0 : (bufferStart + written));
    }

    // Si quedan para escribir, me sigo interesando en escribir
    selector_set_interest(selector, logFileFd, bufferLength > 0 ? OP_WRITE : OP_NOOP);
}

static void fdWriteHandler(struct selector_key* key) {
    tryFlushBufferToFile();
}

static void fdCloseHandler(struct selector_key* key) {
    // Intento enviar lo que queda

    if (bufferLength != 0) {
        // Lo hago bloqueante para que termine, TODO fijarse si esta bien
        int flags = fcntl(logFileFd, F_GETFD, 0);
        fcntl(logFileFd, F_SETFL, flags & (~O_NONBLOCK));
        ssize_t written = write(logFileFd, buffer, bufferLength);
        if (written > 0) {
            bufferLength -= written;
            bufferStart = (bufferLength == 0 ? 0 : (bufferStart + written));
        }
    }

    close(logFileFd);
    logFileFd = -1;
}

static fd_handler fdHandler = {
    .handle_read = NULL,
    .handle_write = fdWriteHandler,
    .handle_close = fdCloseHandler,
    .handle_block = NULL};


static int tryOpenLogfile(const char* logFile, struct tm tm) {
    if (logFile == NULL)
        return -1;

    char logfilebuffer[DEFAULT_LOG_FILE_MAXSTRLEN + 1];

    // Si es "", le ponemos un default (YYYY-MM-DD.log), si ya existe se sigue loggeando ahi
    if (logFile[0] == '\0') {
        snprintf(logfilebuffer, DEFAULT_LOG_FILE_MAXSTRLEN, DEFAULT_LOG_FILE,tm.tm_year + 1900,  tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, tm.tm_min);


        logFile = logfilebuffer;
        

        // Crea la carpeta log si no esta creada
        mkdir(DEFAULT_LOG_FOLDER, LOG_FOLDER_PERMISSION_BITS);
    } else {
        // Agrega el directorio al nombre del archivo
        snprintf(logfilebuffer, DEFAULT_LOG_FILE_MAXSTRLEN, "%s/%s", DEFAULT_LOG_FOLDER, logFile);
        logFile = logfilebuffer;
    }

    int fd = open(logFile, LOG_FILE_OPEN_FLAGS, LOG_FILE_PERMISSION_BITS);
    if (fd < 0) {
        fprintf(stderr, "WARNING: Failed to open logging file at %s. Logging will be disabled.\n", logFile);
        return -1;
    }

    return fd;
}

int loggerInit(fd_selector selectorParam, const char* logFile, FILE* logStreamParam) {
    // Fecha actual para crear el default
    time_t timeNow = time(NULL);
    struct tm tm = *localtime(&timeNow);

    selector = selectorParam;
    logFileFd = selectorParam == NULL ? -1 : tryOpenLogfile(logFile, tm);
    logStream = logStreamParam;
    logLevel = MIN_LOG_LEVEL;

    // Registrar el archivo si lo pudimos abrir
    if (logFileFd >= 0){
        selector_register(selector, logFileFd, &fdHandler, OP_NOOP, NULL);
    }

    // Chequeo que estoy loggeando por archivo o por Stream
    if (logFileFd >= 0 || logStream != NULL) {
        buffer = malloc(LOG_MIN_BUFFER_SIZE);
        bufferCapacity = LOG_MIN_BUFFER_SIZE;
        bufferLength = 0;
        bufferStart = 0;
        if (buffer == NULL) {
            close(logFileFd);
            logFileFd = -1;
            fprintf(stderr, "WARNING: Failed to malloc a buffer for logging\n");
            return -1;
        }
    }

    return 0;
}

int loggerFinalize() {
    if (logFileFd >= 0) {
        selector_unregister_fd(selector, logFileFd); 
        selector = NULL;
    }

    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
        bufferCapacity = 0;
        bufferLength = 0;
        bufferStart = 0;
    }

    logStream = NULL;
    return 0;
}

void loggerSetLevel(TLogLevel level) {
    logLevel = level;
}

int loggerIsEnabledFor(TLogLevel level) {
    return level >= logLevel && (logFileFd > 0 || logStream != NULL);
}

void loggerPrePrint() {
    makeBufferSpace(LOG_BUFFER_MAX_PRINT_LENGTH);
}

void loggerGetBufstartAndMaxlength(char** bufstartVar, size_t* maxlenVar) {
    *bufstartVar = buffer + bufferStart + bufferLength;
    *maxlenVar = bufferCapacity - bufferLength - bufferStart;
}


int loggerPostPrint(int written, size_t maxlen) {
    if (written < 0) {
        fprintf(stderr, "Error: snprintf(): %s\n", strerror(errno));
        return -1;
    }

    if ((size_t)written >= maxlen) {
        fprintf(stderr, "Error: %lu bytes of logs possibly lost\n", written - maxlen + 1);
        written = maxlen - 1;
    }

    if (logStream != NULL) {
        fprintf(logStream, "%s", buffer + bufferStart + bufferLength);
    }

    if (logFileFd >= 0) {
        bufferLength += written;
        tryFlushBufferToFile();
    }
    return 0;
}

#endif // #ifndef DISABLE_LOGGER