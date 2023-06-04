
typedef struct pop3 pop3;

#define GET_POP3(key) ((pop3*) (key)->data)
#define BUFFER_SIZE 1024

typedef enum{
    NONE,
    USER,
    RETR,
    RSET,
    ERROR_COMMAND //para escribir el mensaje de error
} pop3_command;



void pop3_read(struct selector_key* key);
void pop3_write(struct selector_key* key);
void pop3_passive_accept(struct selector_key* key);
void pop3_close(struct selector_key* key);
