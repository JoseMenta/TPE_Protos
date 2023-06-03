#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include "selector.h"
#include "pop3.h"
#include "buffer.h"


//Estados posibles del cliente
typedef enum{
    /*
    * HELLO: estado justo luego de establecer la conexión
    * Se usa para imprimir el mensaje de bienvenida
    */
    HELLO, 
    /*
    * AUTHORIZATION: estado de autorizacion del usuario
    * Se presenta cuando se obtienen las credenciales 
    */
    AUTHORIZATION,
    /*
    * TRANSACTION: estado de transacciones del usuario
    * Se presenta cuando el usuario ejecuta comandos para obtener sus correos
    */
    TRANSACTION,
    /*
    * UPDATE: estado de actualizacion del usuario
    * Se presenta cuando se estan efectuando los cambios realizados por el usuario
    */
    UPDATE,
    /*
    * FINISHED: estado de finalización de la interaccion
    * Se presenta cuando termina la etapa de actualizacion
    */
    FINISHED,
    /*
    * ERROR: estado de error en la sesion
    * Se presenta cuando hay un error en la conexion del usuario,
    * por lo que se liberan todos los recursos asociados a esa conexion
    * 
    * DEJARLO AL FINAL, es considerado el max_state del stm
    */
    ERROR
} pop3_state;


unsigned int hello_write(struct selector_key* key);
unsigned int hello_read(struct selector_key* key);
unsigned int authorization_read(struct selector_key* key);
unsigned int authorization_write(struct selector_key* key);
void authorization_departure(const unsigned state, struct selector_key *key);
unsigned int transaction_write(struct selector_key* key);
unsigned int transaction_read(struct selector_key* key);
void update_arrival(const unsigned state, struct selector_key *key);
unsigned int update_write(struct selector_key* key);
void pop3_destroy(pop3* state);
pop3* pop3_create();


static const struct state_definition state_handlers[] ={
    {
        .state = HELLO,
        .on_read_ready = hello_read,
        .on_write_ready = hello_write
    },
    {
        .state = AUTHORIZATION,
        .on_read_ready = authorization_read,
        .on_write_ready = authorization_write,
        .on_departure = authorization_departure,
    },
    {
        .state = TRANSACTION,
        .on_write_ready = transaction_write,
        .on_read_ready = transaction_read
    },
    {
        .state = UPDATE,
        .on_arrival = update_arrival,
        .on_write_ready = update_write //escribimos el mensaje final
    },
    {
        .state = FINISHED,
    },
    {
        .state = ERROR,
    }
    
};

//fd_handler que van a usar todas las conexiones al servidor (que usen el socket pasivo de pop3)
static const struct fd_handler handler = {
    .handle_read = pop3_read,
    .handle_write = NULL,
    .handle_block = NULL,
    .handle_close = NULL
};

void pop3_passive_accept(struct selector_key* key){
   //Vamos a aceptar la conexion entrante
    pop3* state = NULL;
    struct sockaddr_in address;
    socklen_t address_len;
    const int client_fd = accept(key->fd, (struct sockaddr*) &address,&address_len);

    //Si tuvimos un error al crear el socket activo o no lo pudimos hacer no bloqueante
    if (client_fd == -1 || selector_fd_set_nio(client_fd) == -1)
        goto fail;
    
    if((state = pop3_create())==NULL){
        goto fail;
    }

    //registramos en el selector al nuevo socket, y nos interesamos en escribir para mandarle el mensaje de bienvenida
    //TODO: cambiar a OP_WRITE, lo dejamos asi para el echo 
    if(selector_register(key->s,client_fd,&handler,OP_READ,state)!= SELECTOR_SUCCESS){
        goto fail;
    }

    //Si tenemos metricas, cambiarlas aca

    return;

fail:
    if(client_fd != -1){
        //cerramos el socket del cliente
        close(client_fd);
    }

    //destuyo y libero el pop3
    pop3_destroy(state);
}

//crea la estructura para manejar el estado del servidor
pop3* pop3_create(){
    pop3* ans = calloc(1,sizeof(pop3));
    //inicializar campos
    
    ans->stm.initial = HELLO;
    ans->stm.max_state = ERROR;
    ans->stm.states = state_handlers;
    stm_init(&ans->stm);

    buffer_init(&ans->info_read_buff, BUFFER_SIZE ,ans->read_buff);
    buffer_init(&ans->info_write_buff, BUFFER_SIZE ,ans->read_buff);
    
    return ans;
}

void pop3_read(struct selector_key* key){
    struct state_machine* stm = &(GET_POP3(key)->stm);

    const pop3_state st = stm_handler_read(stm,key);
}


void pop3_write(struct selector_key* key){
    struct state_machine* stm = &(GET_POP3(key)->stm);

    const pop3_state st = stm_handler_write(stm,key); 
}


void pop3_close(struct selector_key* key){
    pop3_destroy(GET_POP3(key));
}

//borrar y liberar el pop3
void pop3_destroy(pop3* state){
    if(state == NULL){
        return;
    }

    //Liberamos la estructura para el estado
    free(state);
}

unsigned hello_read(struct selector_key* key){
    pop3* state = GET_POP3(key);
    
    //Obtenemos el buffer para leer
    size_t max;
    //vamos a guardar lo que leemos en el buffer de entrada
    uint8_t* ptr = buffer_write_ptr(&state->info_write_buff,&max);
    size_t read_count = recv(key->fd, ptr, max, 0);

    if(read_count<=0){
        printf("Error leyendo del socket");
        exit(1);
    }
    for(int i = 0; i<read_count; i++){
        if(ptr[i]=='\n'){
            if( selector_set_interest(key->s,key->fd,OP_NOOP) != SELECTOR_SUCCESS|| 
                selector_set_interest(key->s,key->fd,OP_WRITE) != SELECTOR_SUCCESS)
                printf("Error cambiando el interes del socket para escribir");
                exit(1);
        }
    }
    buffer_write_adv(&state->info_write_buff,read_count);
    return HELLO;
}

unsigned hello_write(struct selector_key* key){
    pop3* state = GET_POP3(key);
    
    size_t max_size;
    uint8_t * ptr = buffer_read_ptr(&state->info_write_buff, &max_size);
    size_t sent_count;

    for(int i = 0; i<max_size || i>= max_size; i++){
        if(ptr[i]=='\n'){
            //TODO: Considerar poner MSG_NOSIGNAL para que no mande señales en el cierrre del socket
            size_t sent_count = send(key->fd, ptr, i, 0);
        }
    }
    if(sent_count == -1){
        printf("Error al escribir en el socket");
        exit(1);
    }

    buffer_read_adv(&state->info_write_buff, sent_count);
    
    

    if(selector_set_interest(key->s, key->fd, OP_NOOP) != SELECTOR_SUCCESS
             || selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS){
                printf("Error cambiando a interes de lectura");
                exit(1);
             }
    
    return HELLO;
}


unsigned authorization_read(struct selector_key* key){}
unsigned authorization_write(struct selector_key* key){}
void authorization_departure(const unsigned state, struct selector_key *key){}
unsigned transaction_write(struct selector_key* key){}
unsigned transaction_read(struct selector_key* key){}
void update_arrival(const unsigned state, struct selector_key *key){}
unsigned update_write(struct selector_key* key){}
