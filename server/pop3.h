#include "stm.h"
#include "buffer.h"

typedef struct pop3 pop3;

#define GET_POP3(key) ((pop3*) (key)->data)
#define BUFFER_SIZE 1024

typedef enum{
    NONE,
    RETR,
    RSET,
    ERROR_COMMAND //para escribir el mensaje de error
} pop3_command;

//Esto es lo que vamos a pasar en void* data del selector
//Vamos a agregar lo que necesitemos, como un array con todos los mails que tiene el usuario
struct pop3{
    pop3_command command;
    struct state_machine stm;

    uint8_t read_buff[BUFFER_SIZE];
    uint8_t write_buff[BUFFER_SIZE];
    buffer info_read_buff;
    buffer info_write_buff;
};

void pop3_read(struct selector_key* key);
void pop3_write(struct selector_key* key);
void pop3_passive_accept(struct selector_key* key);
void pop3_close(struct selector_key* key);
