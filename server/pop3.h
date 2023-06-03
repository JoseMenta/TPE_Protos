
typedef struct pop3 pop3;

#define GET_POP3(key) ((pop3) key->data)

//Esto es lo que vamos a pasar en void* data del selector
struct pop3{
    
};

typedef enum{
    HELLO_STATE
} pop3_state;

void pop3_passive_accept(struct selector_key* key);

