#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include "selector.h"
#include "pop3.h"
#include "buffer.h"
#include "stm.h"
#include "buffer.h"

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

void pop3_done(struct selector_key* key);
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

//fd_handler que van a usar todas las conexiones al servidor (que usen el   socket pasivo de pop3)
static const struct fd_handler handler = {
    .handle_read = pop3_read,
    .handle_write = pop3_write,
    .handle_block = NULL,
    .handle_close = NULL
};

void pop3_passive_accept(struct selector_key* key){
    printf("Se esta aceptando la conexion\n");
   //Vamos a aceptar la conexion entrante
    pop3* state = NULL;
    // Se crea la estructura el socket activo para la conexion entrante para una direccion IPv4
    struct sockaddr_in address;
    socklen_t address_len;
    // Se acepta la conexion entrante, se cargan los datos en el socket activo y se devuelve el fd del socket activo
    // Este fd será tanto para leer como para escribir en el socket activo
    const int client_fd = accept(key->fd, (struct sockaddr*) &address,&address_len);
    printf("Tenemos el fd para la conexion\n");
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
    printf("Registramos al fd en el selector\n");
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
    printf("Se inicializa la etructura\n");
    // Se inicializa la maquina de estados para el cliente
    ans->stm.initial = HELLO;
    ans->stm.max_state = ERROR;
    ans->stm.states = state_handlers;
    stm_init(&ans->stm);

    // Se inicializan los buffers para el cliente (uno para leer y otro para escribir)
    buffer_init(&ans->info_read_buff, BUFFER_SIZE ,ans->read_buff);
    buffer_init(&ans->info_write_buff, BUFFER_SIZE ,ans->read_buff);
    printf("Se termina de inicializar la estructura\n");
    return ans;
}

void pop3_read(struct selector_key* key){
    printf("Intenta leer\n");
    // Se obtiene la maquina de estados del cliente asociado al key
    struct state_machine* stm = &(GET_POP3(key)->stm);
    // Se ejecuta la función de lectura para el estado actual de la maquina de estados
    const pop3_state st = stm_handler_read(stm,key);
    printf("Termina con el estado %d\n",st);

    if(FINISHED == st){
        pop3_done(key);
    }
}


void pop3_write(struct selector_key* key){
    printf("Intenta escribir\n");
    // Se obtiene la maquina de estados del cliente asociado al key
    struct state_machine* stm = &(GET_POP3(key)->stm);
    // Se ejecuta la función de lectura para el estado actual de la maquina de estados
    const pop3_state st = stm_handler_write(stm,key); 
    printf("Termina con el estado %d\n",st);

    if(FINISHED == st){
        pop3_done(key);
    }
}


void pop3_close(struct selector_key* key){
    pop3_destroy(GET_POP3(key));
}


void pop3_done(struct selector_key* key){
    if (selector_unregister_fd(key->s, key->fd) != SELECTOR_SUCCESS) {
        abort();
    }
    close(key->fd);
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
    size_t max = 0;
    //vamos a guardar lo que leemos en el buffer de entrada
    uint8_t* ptr = buffer_write_ptr(&state->info_write_buff,&max);
    // Se lee del socket y se guarda en el buffer de entrada (ptr) hasta un maximo de max bytes
    ssize_t read_count = recv(key->fd, ptr, max, 0);

    printf("estoy tratando de leer\n");
            
    if(read_count<=0){
        printf("Error leyendo del socket\n");
        return FINISHED;
    }
    for(int i = 0; i<read_count; i++){
        printf("Estoy viendo a %c\n",ptr[i]);
        if(ptr[i]=='\n'){
            printf("Vi un fin de linea \n");
            if( selector_set_interest(key->s,key->fd,OP_NOOP) != SELECTOR_SUCCESS|| 
                selector_set_interest(key->s,key->fd,OP_WRITE) != SELECTOR_SUCCESS){
                printf("Error cambiando el interes del socket para escribir\n");
                return FINISHED;
            }
        }
    }
    printf("Todavia no movi el index del buffer para escritura\n");
    buffer_write_adv(&state->info_write_buff,read_count);
    printf("Ya movi el index del buffer para escritura\n");
    return HELLO;
}

unsigned hello_write(struct selector_key* key){
    pop3* state = GET_POP3(key);
    
    printf("Entre en la funcion hello_write\n");

    size_t max_size = 0;
    // Se obtiene el puntero del proximo byte a escribir y la cantidad de bytes disponibles para escribir
    uint8_t * ptr = buffer_read_ptr(&state->info_write_buff, &max_size);
    ssize_t sent_count = 0;

    printf("estoy tratando de escribir\n");

    for(int i = 0; i<max_size; i++){
        if(ptr[i]=='\n'){
            //TODO: Considerar poner MSG_NOSIGNAL para que no mande señales en el cierrre del socket

            // Se escribe por el socket hasta el caracter '\n'
            sent_count = send(key->fd, ptr, i+1, 0);
        }
    }
    if(sent_count == -1){
        printf("Error al escribir en el socket");
        return FINISHED;
    }

    printf("Ya mande el texto con el \n");


    // Se mueve el puntero de lectura del buffer de escritura
    buffer_read_adv(&state->info_write_buff, sent_count);
    
    

    if(selector_set_interest(key->s, key->fd, OP_NOOP) != SELECTOR_SUCCESS
             || selector_set_interest(key->s, key->fd, OP_READ) != SELECTOR_SUCCESS){
                printf("Error cambiando a interes de lectura");
                    return FINISHED;
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
