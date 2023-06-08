#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include "selector.h"
#include "pop3.h"
#include "buffer.h"
#include "stm.h"
#include "maidir_reader.h"
#include "./parserADT/parserADT.h"
#include "args.h"

#define WELCOME_MESSAGE "POP3 server\n"
#define USER_INVALID_MESSAGE "INVALID USER\n"
#define USER_VALID_MESSAGE "OK send PASS\n"
#define PASS_VALID_MESSAGE "OK send IDK\n"
#define PASS_INVALID_MESSAGE "INVALID PASS\n"
#define ERROR_COMMAND_MESSAGE "INVALID COMMAND\n"
#define PASSWD_PATH "/etc/passwd"
#define MAILDIR = "/home/jmentasti/maildir"
//Esto es lo que vamos a pasar en void* data del selector
//Vamos a agregar lo que necesitemos, como un array con todos los mails que tiene el usuario

struct command{
    //chequear argumentos para el comando
    char* name;
    bool (*check)(const char* arg);
    int (*action)(pop3* state);
};

typedef struct command command;

struct authorization{
    char * user;
    char * pass;
    bool user_is_present;
    char * path_to_user_data;
};

struct transaction{
    int i;
};

struct update{
    int i;
};

typedef enum{
    USER = 0,
    PASS,
    STAT,
    LIST,
    RETR,
    DELE,
    NOOP,
    QUIT,
    ERROR_COMMAND //para escribir el mensaje de error
} pop3_command;

typedef enum{
    AUTHORIZATION,
    TRANSACTION,
    UPDATE
}protocol_state;

struct pop3{
    pop3_command command;
    char * arg;
    struct state_machine stm;
    protocol_state pop3_protocol_state;
    uint8_t read_buff[BUFFER_SIZE];
    uint8_t write_buff[BUFFER_SIZE];
    buffer info_read_buff;
    buffer info_write_buff;
    bool finished;
    parserADT parser;
    struct pop3args* pop3_args;
    union{
        struct authorization authorization;
        struct transaction transaction;
        struct update update;
    }state_data;
};

//Estados posibles del cliente
typedef enum{
    /*
    * HELLO: estado justo luego de establecer la conexión
    * Se usa para imprimir el mensaje de bienvenida
    */
    HELLO,
    /*
     * READING_REQUEST: estado donde esta leyendo informacion del socket
     * Se usa para indicar que se tiene que leer del socket
     */
    READING_REQUEST,
    /*
     * WRITING_RESPONSE: estando donde se esta escribiendo la respuesta en el buffer de salida y se escribe en el socket
     * Se usa para llevar la respuesta generada al cliente
     */
    WRITING_RESPONSE,
    /*
     * PROCESSING_RESPONSE: estado donde se esta leyendo de un archivo para hacer el RETR
     * Se usa para dejar los contenidos del archivo en un buffer intermedio, logrando no bloquearse en la lectura del archivo
     */
    PROCESSING_RESPONSE,
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
unsigned int read_request(struct selector_key* key);
unsigned int write_response(struct selector_key* key);
unsigned int process_response(struct  selector_key* key);
void pop3_destroy(pop3* state);
pop3* pop3_create(void * data);
bool have_argument(const char* arg);
bool might_argument(const char* arg);
bool not_argument(const char* arg);
pop3_command get_command(const char* command);
bool check_command_for_protocol_state(protocol_state pop3_protocol_state, pop3_command command);
int user_action(pop3* state);
int pass_action(pop3* state);
int stat_action(pop3* state);
int list_action(pop3* state);
int retr_action(pop3* state);
int dele_action(pop3* state);
int user_validation(pop3* state);
int dele_action(pop3* state);
int noop_action(pop3* state);
int quit_action(pop3* state);
int default_action(pop3* state);


static struct command commands[]={
        {
            .name = "USER",
            .check = have_argument,
            .action = user_action
        },
        {
            .name = "PASS",
            .check = have_argument,
            .action = pass_action
        },
        {
            .name = "STAT",
            .check = not_argument,
            .action = stat_action
        },
        {
            .name = "LIST",
            .check = might_argument,
            .action = list_action
        },
        {
            .name = "RETR",
            .check = have_argument,
            .action = retr_action
        },
        {
            .name = "DELE",
            .check = have_argument,
            .action = dele_action
        },
        {
            .name = "NOOP",
            .check = not_argument,
            .action = noop_action
        },
        {
            .name = "QUIT",
            .check = not_argument,
            .action = quit_action
        },
        {
            .name = NULL, //no deberia llegar aca
            .check = might_argument,
            .action = default_action
        }
};

static const struct state_definition state_handlers[] ={
    {
        .state = HELLO,
        .on_write_ready = hello_write
    },
    {
        .state = READING_REQUEST,
        .on_read_ready = read_request,
    },
    {
        .state = WRITING_RESPONSE,
        .on_write_ready = write_response,
    },
    {
        .state = PROCESSING_RESPONSE,
        .on_read_ready = process_response ,
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

void pop3_passive_accept(struct selector_key* key) {
    printf("Se esta aceptando la conexion\n");
    //Vamos a aceptar la conexion entrante
    pop3 *state = NULL;
    // Se crea la estructura el socket a
    // ctivo para la conexion entrante para una direccion IPv4
    struct sockaddr_storage address;
    socklen_t address_len = sizeof(address);
    // Se acepta la conexion entrante, se cargan los datos en el socket activo y se devuelve el fd del socket activo
    // Este fd será tanto para leer como para escribir en el socket activo
    const int client_fd = accept(key->fd, (struct sockaddr *) &address, &address_len);
    printf("Tenemos el fd para la conexion\n");
    //Si tuvimos un error al crear el socket activo o no lo pudimos hacer no bloqueante
    if (client_fd == -1){
        printf("El fd es: %d\n", key->fd);
        printf("El errno es: %s\n", strerror(errno));
        printf("fallo el accept client\n");
        goto fail;
    }
    if(selector_fd_set_nio(client_fd) == -1) {
        printf("Fallo el flag no bloqueante");
        goto fail;
    }

    if((state = pop3_create(key->data))==NULL){
        printf("Fallo el create");
        goto fail;
    }

    //registramos en el selector al nuevo socket, y nos interesamos en escribir para mandarle el mensaje de bienvenida
    if(selector_register(key->s,client_fd,&handler,OP_WRITE,state)!= SELECTOR_SUCCESS){
        printf("Fallo el registro del socket");
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
pop3* pop3_create(void * data){
    pop3* ans = calloc(1,sizeof(pop3));
    if(ans == NULL || errno == ENOMEM){ //errno por optimismo Linux
        printf("Error al reservar memoria para el estado");
        return NULL;
    }
    // Se inicializa la maquina de estados para el cliente
    ans->pop3_protocol_state = AUTHORIZATION;
    ans->stm.initial = HELLO;
    ans->stm.max_state = ERROR;
    ans->stm.states = state_handlers;
    stm_init(&ans->stm);
    ans->parser = parser_init();
    ans->pop3_args = (struct pop3args*) data;

    // Se inicializan los buffers para el cliente (uno para leer y otro para escribir)
    buffer_init(&(ans->info_read_buff), BUFFER_SIZE ,ans->read_buff);
    buffer_init(&(ans->info_write_buff), BUFFER_SIZE ,ans->write_buff);

    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(ans->info_write_buff),&max);
    strncpy((char*)ptr,WELCOME_MESSAGE,max);
    buffer_write_adv(&(ans->info_write_buff),strlen(WELCOME_MESSAGE));

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
    pop3_destroy(GET_POP3(key));
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

unsigned hello_write(struct selector_key* key){
    pop3* state = GET_POP3(key);
    size_t  max = 0;
    uint8_t* ptr = buffer_read_ptr(&(state->info_write_buff),&max);
    ssize_t sent_count = send(key->fd,ptr,max,0);

    if(sent_count == -1){
        printf("Error al escribir en el socket");
        return FINISHED;
    }
    buffer_read_adv(&(state->info_write_buff),sent_count);

    //Si ya no hay mas para escribir y el comando termino de generar la respuesta
    if(!buffer_can_read(&(state->info_write_buff))){
        if(selector_set_interest(key->s,key->fd,OP_READ) != SELECTOR_SUCCESS){
            printf("Error cambiando el interes del socket para leer\n");
            return FINISHED;
        }
    }
    return READING_REQUEST;
}

unsigned int read_request(struct selector_key* key){
    printf("Entra a leer el request!!\n");
    pop3* state = GET_POP3(key);
    //Guardamos lo que leemos del socket en el buffer de entrada
    size_t max = 0;
    uint8_t* ptr = buffer_write_ptr(&(state->info_read_buff),&max);
    ssize_t read_count = recv(key->fd, ptr, max, 0);

    if(read_count<=0){
        printf("Error leyendo del socket\n");
        return FINISHED;
    }
    //Avanzamos la escritura en el buffer
    buffer_write_adv(&(state->info_read_buff),read_count);
    //Obtenemos un puntero para lectura
    ptr = buffer_read_ptr(&(state->info_read_buff),&max);
    for(size_t i = 0; i<max; i++){
        parser_state parser = parser_feed(state->parser, ptr[i]);
        if(parser == PARSER_FINISHED || parser == PARSER_ERROR){
            //avanzamos solo hasta el fin del comando
            buffer_read_adv(&(state->info_read_buff),i+1);
            const char* parser_command = get_cmd(state->parser);
            pop3_command command = get_command(parser_command);
            state->command = command;
            state->arg = get_arg(state->parser);
            if(parser == PARSER_ERROR || command == ERROR_COMMAND || !commands[command].check(get_arg(state->parser))){
                printf("No es un comando valido");
                state->command = ERROR_COMMAND;
            }
            if(!check_command_for_protocol_state(state->pop3_protocol_state, command)){
                printf("Comando no permitido\n");
                state->command = ERROR_COMMAND;
            }
            parser_reset(state->parser);
            //Vamos a procesar la respuesta
            if(selector_set_interest(key->s,key->fd,OP_WRITE) != SELECTOR_SUCCESS){
                printf("Error cambiando el interes del socket para escribir\n");
                return FINISHED;
            }
            return WRITING_RESPONSE; //vamos a escribir la respuesta
        }
    }
    //Avanzamos en el buffer, leimos lo que tenia
    buffer_read_adv(&(state->info_read_buff),max);
    printf("Ya movi el index del buffer para escritura\n");
    return READING_REQUEST; //vamos a seguir leyendo el request
}
unsigned int write_response(struct selector_key* key){
    printf("Entra a escribir el response!!\n");
    pop3* state = GET_POP3(key);
    //ejecutamos la funcion para generar la respuesta, que va a setear a state->finished como corresponda
    command current_command = commands[state->command];
    //TODO: hacer que devuelva a donde tenemos que ir para el caso de RETR
    current_command.action(state); //ejecutamos la accion

    //Escribimos lo que tenemos en el buffer de salida al socket
    size_t  max = 0;
    uint8_t* ptr = buffer_read_ptr(&(state->info_write_buff),&max);
    ssize_t sent_count = send(key->fd,ptr,max,0);

    if(sent_count == -1){
        printf("Error al escribir en el socket");
        return FINISHED;
    }
    buffer_read_adv(&(state->info_write_buff),sent_count);
    printf("Avanzamos el puntero de lectura en el buffer de escritura\n");
    //Si ya no hay mas para escribir y el comando termino de generar la respuesta
    if(!buffer_can_read(&(state->info_write_buff)) && state->finished){
        state->finished = false;
        //Terminamos de mandar la respuesta para el comando, vemos si nos queda otro
        size_t  max = 0;
        uint8_t* ptr = buffer_read_ptr(&(state->info_read_buff),&max);
        for(size_t i = 0; i<max; i++){
            parser_state parser = parser_feed(state->parser, ptr[i]);
            if(parser == PARSER_FINISHED || parser == PARSER_ERROR){
                //avanzamos solo hasta el fin del comando
                buffer_read_adv(&(state->info_read_buff),i+1);
                const char* parser_command = get_cmd(state->parser);
                pop3_command command = get_command(parser_command);
                state->command = command;
                if(parser == PARSER_ERROR || command == ERROR_COMMAND || !commands[command].check(get_arg(state->parser))){
                    printf("No es un comando valido");
                    state->command = ERROR_COMMAND;
                }
                if(!check_command_for_protocol_state(state->pop3_protocol_state, command)){
                    printf("Comando no permitido\n");
                    state->command = ERROR_COMMAND;
                }
                parser_reset(state->parser);
                return WRITING_RESPONSE; //vamos a escribir la respuesta
            }
        }
        buffer_read_adv(&(state->info_read_buff),max);
        //No hay un comando completo, volvemos a leer
        if(selector_set_interest(key->s,key->fd,OP_READ) != SELECTOR_SUCCESS){
            printf("Error cambiando el interes del socket para leer\n");
            return FINISHED;
        }
        printf("Cambio al interes de lectura\n");
        return READING_REQUEST;
    }
    //Va a volver a donde esta, tiene que seguir escribiendo
    return WRITING_RESPONSE;
}
unsigned int process_response(struct  selector_key* key){
    return 0;
}

bool check_command_for_protocol_state(protocol_state pop3_protocol_state, pop3_command command){
    switch (pop3_protocol_state) {
        case AUTHORIZATION:
            return command == QUIT || command == USER || command == PASS;
        case TRANSACTION:
            return command != USER && command!=PASS;
        case UPDATE:
            return false; //no deberia llegar con un comando aca
        default:
            return false;
    }
}

bool have_argument(const char* arg){
    printf("Entro en la funcion de have_argument con el argumento '%s'\n", arg);
    bool aux = strlen(arg)!=0;
    if(aux){
        printf("true");
    }else{
        printf("false");
    }
    return aux;
}

bool might_argument(const char* arg){
    printf("Entro en la funcion de might_argument con el argumento '%s'\n", arg);
    return true;
}

bool not_argument(const char* arg){
    printf("Entro a la funcion not_argument con el argumento '%s'\n", arg);
    return arg == NULL || strlen(arg) == 0;
}

pop3_command get_command(const char* command){
    for(int i = USER; i<QUIT; i++){
        if(strncasecmp(command,commands[i].name,4)==0){
            return i;
        }
    }
    return ERROR_COMMAND;
}

int user_action(pop3* state){
    printf("Entro a user_action\n");
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(state->info_write_buff),&max);
    char * msj = USER_INVALID_MESSAGE;
    for(unsigned int i=0; i<state->pop3_args->users->users_count; i++){
        if(strcmp(state->arg, state->pop3_args->users->users_array[i].name) == 0){
            state->state_data.authorization.user = state->pop3_args->users->users_array[i].name;
            state->state_data.authorization.pass = state->pop3_args->users->users_array[i].pass;
            msj = USER_VALID_MESSAGE;
        }
    }
    strncpy((char*)ptr, msj, max);
    buffer_write_adv(&(state->info_write_buff),strlen(msj));
    state->finished = true;
    printf("Salgo de user_action\n");
    return 0;
}

int pass_action(pop3* state){
    printf("Entro a pass action\n");
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(state->info_write_buff),&max);
    char * msj = PASS_INVALID_MESSAGE;
    if(state->state_data.authorization.pass != NULL && strcmp(state->arg, state->state_data.authorization.pass) == 0){
        msj = PASS_VALID_MESSAGE;
        state->pop3_protocol_state = TRANSACTION;
    }
    strncpy((char*)ptr, msj,max);
    buffer_write_adv(&(state->info_write_buff),strlen(msj));
    state->finished = true;
    printf("Salgo de pass_action\n");
    return  0;
}

int stat_action(pop3* state){
    printf("Entro a stat action\n");
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(state->info_write_buff),&max);
    strncpy((char*)ptr, WELCOME_MESSAGE, max);
    buffer_write_adv(&(state->info_write_buff),strlen(WELCOME_MESSAGE));
    state->finished = true;
    return  0;
}
int default_action(pop3* state){
    printf("Entro a default action\n");
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(state->info_write_buff),&max);
    strncpy((char*)ptr, ERROR_COMMAND_MESSAGE, max);
    buffer_write_adv(&(state->info_write_buff),strlen(ERROR_COMMAND_MESSAGE));
    state->finished = true;
    return 0;
}

int list_action(pop3* state){
    return 0;
}
int retr_action(pop3* state){
    return 0;
}
int user_validation(pop3* state){
    return 0;
}
int dele_action(pop3* state){
    return 0;
}
int noop_action(pop3* state){
    return 0;
}
int quit_action(pop3* state){
    return 0;
}
