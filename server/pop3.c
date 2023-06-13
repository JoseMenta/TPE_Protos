#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include "selector.h"
#include "pop3.h"
#include "buffer.h"
#include "stm.h"
#include "maidir_reader.h"
#include "./parser/parserADT.h"
#include "./parser/parser_definition/pop3_parser_definition.h"
#include "./parser/parser_definition/byte_stuffing_parser_definition.h"
#include "args.h"

#define MAX_CMD 5
#define MAX_ARG 41
#define WELCOME_MESSAGE "POP3 server\r\n"
#define USER_INVALID_MESSAGE "-ERR INVALID USER\r\n"
#define USER_VALID_MESSAGE "+OK send PASS\r\n"
#define PASS_VALID_MESSAGE "+OK\r\n"
#define PASS_INVALID_MESSAGE "INVALID PASS\r\n"
#define OK_MESSSAGE "+OK\r\n"
#define ERROR_MESSSAGE "-ERR\r\n"
#define ERROR_COMMAND_MESSAGE "-ERR INVALID COMMAND\r\n"
#define ERROR_RETR_MESSAGE "INVALID MESSEGE NUMBER\r\n"
#define ERROR_RETR_ARG_MESSEGE "MISSING MESSEGE NUMBER\r\n"
#define ERROR_DELETED_MESSAGE "-ERR THIS MESSAGE IS DELETED\r\n"
#define ERROR_INDEX_MESSAGE "-ERR no such message\r\n"
#define QUIT_MESSAGE "+OK Logging out\r\n"
#define CAPA_MESSAGE "+OK Capability list follows\r\nUSER\r\nPIPELINING\r\n.\r\n"
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

typedef enum{
    MULTILINE_STATE_FIRST_LINE,
    MULTILINE_STATE_MULTILINE,
    MULTILINE_STATE_END_LINE,
}multiline_state;

struct transaction{
    int mail_index;
    bool has_arg;
    bool arg_processed;
    long arg;
    multiline_state multiline_state;
    bool file_opened;
    bool file_ended;
    int file_fd;
    int flag;
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
    RSET,
    QUIT,
    CAPA,
    ERROR_COMMAND //para escribir el mensaje de error
} pop3_command;

typedef enum{
    AUTHORIZATION,
    TRANSACTION,
}protocol_state;

struct pop3{
    int connection_fd;
    pop3_command command;
    char  arg[MAX_ARG];
    char  cmd[MAX_CMD];
    struct state_machine stm;
    protocol_state pop3_protocol_state;
    uint8_t read_buff[BUFFER_SIZE];
    uint8_t write_buff[BUFFER_SIZE];
    uint8_t file_buff[BUFFER_SIZE];
    buffer info_file_buff;
    buffer info_read_buff;
    buffer info_write_buff;
    bool finished;
    parserADT pop3_parser;
    parserADT byte_stuffing_parser;
    email* emails;
    size_t emails_count;
    char* path_to_user_maildir;
    int references;
    struct pop3args* pop3_args;
    char * user;
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

//funciones utilizadas por la stm
unsigned int hello_write(struct selector_key* key);
unsigned int read_request(struct selector_key* key);
unsigned int write_response(struct selector_key* key);
unsigned int process_response(struct  selector_key* key);
void finish_connection(const unsigned state, struct selector_key *key);
void process_open_file(const unsigned state, struct selector_key *key);
//funcion para reiniciar estructuras asociadas a un estado del protocolo
void reset_structures(pop3* state);
//funciones utilizadas para crear y destruir la estructura que mantiene el estado de una conexion (pop3)
void pop3_destroy(pop3* state);
pop3* pop3_create(void * data);
//funciones auxiliares para las estructuras de los comandos de pop3
bool have_argument(const char* arg);
bool might_argument(const char* arg);
bool not_argument(const char* arg);
pop3_command get_command(const char* command);
bool check_command_for_protocol_state(protocol_state pop3_protocol_state, pop3_command command);
//acciones asociadas a un comando de pop3
//TODO: que reciban a selector_key*
int user_action(pop3* state);
int pass_action(pop3* state);
int stat_action(pop3* state);
int list_action(pop3* state);
int retr_action(pop3* state);
int dele_action(pop3* state);
int noop_action(pop3* state);
int quit_action(pop3* state);
int default_action(pop3* state);
int rset_action(pop3* state);
int capa_action(pop3* state);
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
        },{
            .name = "RSET",
            .check = not_argument,
            .action = rset_action
        },
        {
            .name = "QUIT",
            .check = not_argument,
            .action = quit_action
        },
        {
            .name = "CAPA",
            .check = not_argument,
            .action = capa_action,
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
        .on_arrival = process_open_file,
        .on_read_ready = process_response ,
    },
    {
        .state = FINISHED,
        .on_arrival = finish_connection,
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
    .handle_close = pop3_close //se llama incluso cuando cierra el servidor
};

/*
 * Funcion utilizada en el socket pasivo para aceptar una nueva conexion y agregarla al selector
 */
void pop3_passive_accept(struct selector_key* key) {
    printf("Se esta aceptando la conexion\n");
    //Vamos a aceptar la conexion entrante
    pop3 *state = NULL;
    // Se crea la estructura del socket
    // activo para la conexion entrante
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
    state->connection_fd = client_fd;
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

/*
 * Funcion utilizada para crear la estructura pop3 que mantiene el estado de una conexion
 */
pop3* pop3_create(void * data){
    extern const parser_definition pop3_parser_definition;
    extern const parser_definition byte_stuffing_parser_definition;
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
    ans->pop3_parser = parser_init(&pop3_parser_definition);
    ans->byte_stuffing_parser = parser_init(&byte_stuffing_parser_definition);
    ans->pop3_args = (struct pop3args*) data;

    // Se inicializan los buffers para el cliente (uno para leer y otro para escribir)
    buffer_init(&(ans->info_read_buff), BUFFER_SIZE ,ans->read_buff);
    buffer_init(&(ans->info_write_buff), BUFFER_SIZE ,ans->write_buff);
    buffer_init(&(ans->info_file_buff), BUFFER_SIZE, ans->file_buff);
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(ans->info_write_buff),&max);
    strncpy((char*)ptr,WELCOME_MESSAGE,max);
    buffer_write_adv(&(ans->info_write_buff),strlen(WELCOME_MESSAGE));

    printf("Se termina de inicializar la estructura\n");
    return ans;
}

/*
 * Funcion para liberar memoria asociada a la estructura pop3
 */
void pop3_destroy(pop3* state){
    if(state == NULL){
        return;
    }
    //Si hay campos adentro que liberar, hacerlo aca
    parser_destroy(state->pop3_parser);
    parser_destroy(state->byte_stuffing_parser);
    free_emails(state->emails,state->emails_count);
    if(state->path_to_user_maildir != NULL){
        free(state->path_to_user_maildir);
    }
    free(state);
}
/*
 * --------------------------------------------------------------------------------------
 * Funciones utilizadas por el selector ante los eventos de lectura, escritura y cierre
 * --------------------------------------------------------------------------------------
 */
/*
 * Funcion llamada por el selctor cuando puede leer de un fd
 */
void pop3_read(struct selector_key* key){
    printf("Intenta leer\n");
    // Se obtiene la maquina de estados del cliente asociado al key
    struct state_machine* stm = &(GET_POP3(key)->stm);
    // Se ejecuta la función de lectura para el estado actual de la maquina de estados
    const pop3_state st = stm_handler_read(stm,key);
    printf("Termina con el estado %d\n",st);
}
/*
 * Funcion llamada por el selector cuando puede escribir en un fd
 */
void pop3_write(struct selector_key* key){
    printf("Intenta escribir\n");
    // Se obtiene la maquina de estados del cliente asociado al key
    struct state_machine* stm = &(GET_POP3(key)->stm);
    // Se ejecuta la función de lectura para el estado actual de la maquina de estados
    const pop3_state st = stm_handler_write(stm,key);
    printf("Termina con el estado %d\n",st);
}
/*
 * Funcion llamada por el selector cuando se llama a selector_unregister_fd (es decir, cuando se saca al fd del selector)
 */
void pop3_close(struct selector_key* key){
    pop3 *data = GET_POP3(key);
    close(key->fd);
    //No puedo determinar el orden en el que se llama (puede ser primero con el archivo o con la conexion)
    //por lo que usamos references para que se libere al estado de la conexion en el ultimo que se llama
    if(data->references==0){
        //soy el ultimo con referencia al estado, lo libero
        pop3_destroy(data);
    }else{
        (data->references)--;
    }
}


/*
 * --------------------------------------------------------------------------------------
 * Funciones utilizadas por la stm para sus estados
 * --------------------------------------------------------------------------------------
 */

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
    //si no pude mandar el mensaje de bienvenida completo
    if(buffer_can_read(&(state->info_write_buff))){
        return HELLO;
    }
    //Si ya no hay mas para escribir y termine con el mensaje de bienvenida
    if(selector_set_interest(key->s,key->fd,OP_READ) != SELECTOR_SUCCESS){
        printf("Error cambiando el interes del socket para leer\n");
        return FINISHED;
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
        parser_state parser = parser_feed(state->pop3_parser, ptr[i]);
        if(parser == PARSER_FINISHED || parser == PARSER_ERROR){
            //avanzamos solo hasta el fin del comando
            buffer_read_adv(&(state->info_read_buff),i+1);
            pop3_parser_data * parser_data = (pop3_parser_data *) parser_get_data(state->pop3_parser);
            if (parser_data == NULL) {
                printf("Error obteniendo los datos del parser\n");
                return FINISHED;
            }
            const char* parser_command = parser_data->cmd;
            //TODO: chequear bien error
            if(parser_command == NULL){
                return FINISHED;
            }
            pop3_command command = get_command(parser_command);
            state->command = command;
            state->arg = get_pop3_arg(parser_data);
            if (state->arg == NULL) {
                printf("Error obteniendo el argumento del comando\n");
                return FINISHED;
            }
            free((void*)parser_data);
            if(parser == PARSER_ERROR || command == ERROR_COMMAND || !commands[command].check(state->arg)){
                printf("No es un comando valido");
                state->command = ERROR_COMMAND;
            }
            if(!check_command_for_protocol_state(state->pop3_protocol_state, command)){
                printf("Comando no permitido\n");
                state->command = ERROR_COMMAND;
            }
            parser_reset(state->pop3_parser);
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
    if(!state->finished) {
        unsigned int ret_state = current_command.action(state); //ejecutamos la accion
        //Si tengo que irme de este estado (para leer del archivo) me voy
        if(ret_state!=WRITING_RESPONSE){
            //Dejo de suscribirme en donde estoy, tengo que ir a otro lado
            if(selector_set_interest(key->s,key->fd,OP_NOOP) != SELECTOR_SUCCESS){
                printf("Error cambiando el interes del socket para leer\n");
                return FINISHED;
            }
            return ret_state;
        }
    }
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
            parser_state parser = parser_feed(state->pop3_parser, ptr[i]);
            if(parser == PARSER_FINISHED || parser == PARSER_ERROR){
                //avanzamos solo hasta el fin del comando
                buffer_read_adv(&(state->info_read_buff),i+1);
                pop3_parser_data * parser_data = (pop3_parser_data *) parser_get_data(state->pop3_parser);
                if (parser_data == NULL) {
                    printf("Error obteniendo los datos del parser\n");
                    return FINISHED;
                }
                const char* parser_command = parser_data->cmd;
                //TODO: chequear bien error
                if(parser_command == NULL){
                    return FINISHED;
                }
                pop3_command command = get_command(parser_command);
                state->command = command;
                state->arg = get_pop3_arg(parser_data);
                if (state->arg == NULL) {
                    printf("Error obteniendo el argumento del comando\n");
                    return FINISHED;
                }
                free((void*)parser_data);
                if(parser == PARSER_ERROR || command == ERROR_COMMAND || !commands[command].check(state->arg)){
                    printf("No es un comando valido");
                    state->command = ERROR_COMMAND;
                }
                if(!check_command_for_protocol_state(state->pop3_protocol_state, command)){
                    printf("Comando no permitido\n");
                    state->command = ERROR_COMMAND;
                }
                parser_reset(state->pop3_parser);
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

void finish_connection(const unsigned state, struct selector_key *key){
    pop3 * data = GET_POP3(key);
    if(data->pop3_protocol_state == TRANSACTION && data->state_data.transaction.file_opened){
        //Cierro el archivo, lo saco del selector
        if(selector_unregister_fd(key->s, data->state_data.transaction.file_fd) != SELECTOR_SUCCESS){
            abort();
        }
    }
    if (selector_unregister_fd(key->s, key->fd) != SELECTOR_SUCCESS) {
        abort();
    }
    //con selector_unregister_fd, se va a llamar a la funcion pop3_close
}

/*
 * --------------------------------------------------------------------------------------
 * Funciones auxiliares para los comandos (struct command)
 * --------------------------------------------------------------------------------------
 */
void reset_structures(pop3* state){
    memset(&(state->state_data),0,sizeof(state->state_data));
    //Si hay que dejar alguna cosa distinta, hacerlo aca
}

bool check_command_for_protocol_state(protocol_state pop3_protocol_state, pop3_command command){
    switch (pop3_protocol_state) {
        case AUTHORIZATION:
            return command == QUIT || command == USER || command == PASS || command == CAPA;
        case TRANSACTION:
            return command != USER && command!=PASS;
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
    for(int i = USER; i<=CAPA; i++){
        if(strncasecmp(command,commands[i].name,4)==0){
            return i;
        }
    }
    return ERROR_COMMAND;
}

typedef enum{
    TRY_PENDING,
    TRY_DONE
}try_state;
/*
 * Funcion auxiliar para escribir el string str al buffer buff si tiene espacio
 *
 * Return
 * Devuelve TRY_DONE si se pudo escribir el string completo en el buffer, TRY_PENDING si no
 * (y en ese caso no escribe parte del string)
 */
try_state try_write(const char* str, buffer* buff){
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(buff,&max);
    size_t message_len = strlen(str);
    if(max<message_len){
        //vuelvo a intentar despues
        return TRY_PENDING;
    }
    //Manda el mensaje parcialmente si no hay espacio
    strncpy((char*)ptr, str, message_len);
    buffer_write_adv(buff,(ssize_t)message_len);
    return TRY_DONE;
}

/*
 * Acciones asociadas a cada comando de pop3
 */
int user_action(pop3* state){
    char * msj = USER_INVALID_MESSAGE;
    for(unsigned int i=0; i<state->pop3_args->users->users_count; i++){
        if(strcmp(state->arg, state->pop3_args->users->users_array[i].name) == 0){
            state->state_data.authorization.user = state->pop3_args->users->users_array[i].name;
            state->state_data.authorization.pass = state->pop3_args->users->users_array[i].pass;
            msj = USER_VALID_MESSAGE;
        }
    }
    if(try_write(msj,&(state->info_write_buff)) == TRY_PENDING){
        //No deberia pasar nunca, si llego aca es porque el buffer de salida esta vacio
        printf("En user_action se intenta escribir al buffer y no se puede\n");
        return FINISHED;
    }
    state->finished = true;
    return WRITING_RESPONSE;
}

int pass_action(pop3* state){
    char * msj = PASS_INVALID_MESSAGE;
    if(state->state_data.authorization.pass != NULL && strcmp(state->arg, state->state_data.authorization.pass) == 0){
        if()
        msj = PASS_VALID_MESSAGE;
        state->user= state->state_data.authorization.user;
        state->pop3_protocol_state = TRANSACTION;
        state->path_to_user_maildir = usersADT_get_user_mail_path(state->pop3_args->users,state->pop3_args->maildir_path, state->state_data.authorization.user);
        //TODO: Setear el max para el mails con cliente
        size_t mails_max = 20;
        state->emails = read_maildir(state->path_to_user_maildir,&mails_max);
        if(state->emails == NULL){
            //TODO: ver de imprimir un error y mantener la conexion
            printf("No hay cosas en el directorio de mails!\n");
            return FINISHED;
        }
        state->emails_count = mails_max;
        reset_structures(state);
    }
    if(try_write(msj,&(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir en el buffer en pass_action y no tengo espacio");
        return FINISHED;
    }
    state->finished = true;
    return  WRITING_RESPONSE;
}

int stat_action(pop3* state){
    //TODO: reemplazar con constante
    int max_len = 3+1+20+1+20+3;
    //Puedo escribir la respuesta
    char aux[max_len];
    //computamos el total de size
    long aux_len_emails = 0;
    for(size_t i=0; i<state->emails_count ; i++){
        if(!state->emails[i].deleted){
            aux_len_emails += state->emails[i].size;
        }
    }
    //TODO: no contar nos eliminados
    snprintf(aux,max_len,"+OK %zu %ld\r\n",state->emails_count,aux_len_emails);
    if(try_write(aux,&(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir en el buffer en stat_action y no tengo espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return  WRITING_RESPONSE;
}
int default_action(pop3* state){
    if(try_write(ERROR_COMMAND_MESSAGE,&(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir en el buffer en default_action y no tengo espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return WRITING_RESPONSE;
}


int list_action(pop3* state){
    //procesamos el argumento recibido
    if(!state->state_data.transaction.arg_processed && strlen(state->arg) != 0){
        state->state_data.transaction.has_arg = true;
        state->state_data.transaction.arg = strtol(state->arg, NULL,10);
    }else{
        state->state_data.transaction.has_arg = false;
    }
    state->state_data.transaction.arg_processed = true;
    if(state->state_data.transaction.multiline_state == MULTILINE_STATE_FIRST_LINE){
        //estamos escribiendo la primera linea, ya sea de error u OK
        if(state->state_data.transaction.has_arg){
            if(state->state_data.transaction.arg > state->emails_count || state->state_data.transaction.arg <= 0){
                //Error de indice
                if(try_write(ERROR_INDEX_MESSAGE,&(state->info_write_buff)) == TRY_PENDING){
                    printf("Intento escribir la primera linea en list_action y no hay espacio en el buffer\n");
                    return FINISHED;
                }
            }else if(state->emails[state->state_data.transaction.arg-1].deleted){
                if(try_write(ERROR_DELETED_MESSAGE,&(state->info_write_buff)) == TRY_PENDING){
                    printf("Intento escribir la primera linea en list_action y no hay espacio en el buffer\n");
                    return FINISHED;
                }
            }else {
                //Tengo que mostrar solo la informacion de ese mail
                int message_len = 3 + 20 + 1 + 20 + 3;
                char aux[message_len];
                email send_email = state->emails[state->state_data.transaction.arg - 1];
                snprintf(aux, message_len, "+OK %ld %ld\r\n", state->state_data.transaction.arg, send_email.size);
                if (try_write(aux, &(state->info_write_buff)) == TRY_PENDING) {
                    printf("Intento escribir la primera linea en list_action y no hay espacio en el buffer\n");
                    return FINISHED;
                }
            }
            state->finished = true;
            reset_structures(state);
            return WRITING_RESPONSE;
        }else{
            //Tengo que escribir la primera linea de una respuesta multilinea
            int message_len = 3 + 1 + 20 + 1+ 8 + 3;
            char aux[message_len];
            snprintf(aux,message_len,"+OK %zu messages\r\n", state->emails_count);
            if (try_write(aux, &(state->info_write_buff)) == TRY_PENDING) {
                printf("Intento escribir la primera linea en list_action y no hay espacio en el buffer\n");
                return FINISHED;
            }
            state->state_data.transaction.multiline_state = MULTILINE_STATE_MULTILINE;
            state->state_data.transaction.mail_index = 0;
        }
    }
    if(state->state_data.transaction.multiline_state==MULTILINE_STATE_MULTILINE){
        //Tengo que imprimir todos los mails
        for(; state->state_data.transaction.mail_index<state->emails_count; state->state_data.transaction.mail_index++){
            if(state->emails[state->state_data.transaction.mail_index].deleted){
                continue;
            }
            int message_len = 20+1+20+3;
            char aux[message_len];
            email send_email = state->emails[state->state_data.transaction.mail_index];
            snprintf(aux,message_len,"%d %ld\r\n",state->state_data.transaction.mail_index+1,send_email.size);
            if (try_write(aux, &(state->info_write_buff)) == TRY_PENDING) {
                //No entra la linea de este mail en el buffer
                //sigo intentando despues
                return WRITING_RESPONSE;
            }
        }
        state->state_data.transaction.multiline_state = MULTILINE_STATE_END_LINE;
    }
    if(state->state_data.transaction.multiline_state==MULTILINE_STATE_END_LINE){
        if (try_write(".\r\n", &(state->info_write_buff)) == TRY_PENDING) {
            //No entra la linea final en el buffer
            //sigo intentando despues
            return WRITING_RESPONSE;
        }
        reset_structures(state);
        state->finished = true;
    }
    return WRITING_RESPONSE;
}

//TODO: pasar a un parser basico
int get_flag(int curr_flag, char curr_char){
    switch (curr_char) {
        case '\r':
            return 1;
        case '\n':
            return curr_flag==1?2:0;
        case '.':
            return curr_flag==2?3:0;
        default:
            return 0;
    }
}

int retr_action(pop3* state){
    if(!state->state_data.transaction.arg_processed && strlen(state->arg) != 0){
        state->state_data.transaction.has_arg = true;
        state->state_data.transaction.arg = strtol(state->arg, NULL,10);
        //TODO: manejar el caso donde no se puede convertir el argumento
    }else{
        state->state_data.transaction.has_arg = false;
    }
    size_t max = 0;
    uint8_t * ptr = buffer_write_ptr(&(state->info_write_buff),&max);
    if(state->state_data.transaction.multiline_state == MULTILINE_STATE_FIRST_LINE){
        if(!state->state_data.transaction.has_arg){
            if(try_write(ERROR_RETR_ARG_MESSEGE, &(state->info_write_buff)) == TRY_PENDING){
                printf("Intento escribir la primera linea en retr_action y no hay espacio\n");
                return FINISHED;
            }
            state->finished = true;
            reset_structures(state);
            return WRITING_RESPONSE;
        }else if(state->state_data.transaction.arg > state->emails_count || state->state_data.transaction.arg <=0){
            if(try_write(ERROR_RETR_MESSAGE, &(state->info_write_buff)) == TRY_PENDING){
                printf("Intento escribir la primera linea en retr_action y no hay espacio\n");
                return FINISHED;
            }
            state->finished = true;
            reset_structures(state);
            return WRITING_RESPONSE;
        }else if(state->emails[state->state_data.transaction.arg-1].deleted){
            if(try_write(ERROR_DELETED_MESSAGE, &(state->info_write_buff)) == TRY_PENDING){
                printf("Intento escribir la primera linea en retr_action y no hay espacio\n");
                return FINISHED;
            }
            state->finished = true;
            reset_structures(state);
            return  WRITING_RESPONSE;
        }else{
            int message_size = 3+1+20+1+6+3;
            char aux[3+20+6+3];
            snprintf(aux,message_size,"+OK %ld octets\r\n",state->emails[state->state_data.transaction.arg-1].size);
            if(try_write(aux, &(state->info_write_buff)) == TRY_PENDING){
                printf("Intento escribir la primera linea en retr_action y no hay espacio\n");
                return FINISHED;
            }
            state->state_data.transaction.multiline_state = MULTILINE_STATE_MULTILINE;
            state->state_data.transaction.flag = 2;//TODO: cambiar a la maquina de estados y poner como inicial que vio \r\n
        }
    }
    if(state->state_data.transaction.multiline_state == MULTILINE_STATE_MULTILINE){
        //Tengo que empezar a leer el archivo
        //si no abri el archivo o no tengo mas para leer pero no lo termine
        if(!state->state_data.transaction.file_opened || (!buffer_can_read(&(state->info_file_buff)) && !state->state_data.transaction.file_ended)) {
            //Tenemos que abrir el archivo y nos interesamos para leer de el
            return PROCESSING_RESPONSE;
        }
        //Tengo cosas en el buffer del archivo para leer
        size_t write_max = 0;
        uint8_t *write_ptr = buffer_write_ptr(&(state->info_write_buff), &write_max);
        size_t file_max = 0;
        uint8_t *file_ptr = buffer_read_ptr(&(state->info_file_buff), &file_max);
        //siempre me quedo con al menos 2 espacios en el de write por si tengo que hacer byte stuffing
        size_t write = 0, file = 0;
        for (; file < file_max && write < write_max - 1; write++, file++) {
            if(PARSER_ACTION == parser_feed(state->byte_stuffing_parser, file_ptr[file])) {
                //tengo que hacer byte stuffing
                //vi un punto al inicio de una linea nueva
                write_ptr[write++] = '.'; //agrego un punto al principio
            }
            write_ptr[write] = file_ptr[file];
        }
        //No avanzar el maximo, por si se queda un caracter en el buffer del archivo que no se procesa por no tener 2 espacios en el de salida
        buffer_write_adv(&(state->info_write_buff), (ssize_t)write);
        buffer_read_adv(&(state->info_file_buff), (ssize_t)file);
        //TODO: revisar, creo que esta es la condicion bajo la cual ya termino de procesar todo el archivo
        //porque solo lee de file_buff si puede escribir lo que queda en write_buf
        if(!buffer_can_read(&(state->info_file_buff)) && state->state_data.transaction.file_ended){
            state->state_data.transaction.multiline_state = MULTILINE_STATE_END_LINE;
        }
    }
    if(state->state_data.transaction.multiline_state == MULTILINE_STATE_END_LINE){
        //podemos no llegar a poder escribir la ultima linea por lo de arriba, entonces probamos
        if(try_write("\r\n.\r\n", &(state->info_write_buff)) == TRY_PENDING){
            return WRITING_RESPONSE;
        }
        parser_reset(state->byte_stuffing_parser);
        reset_structures(state);
        state->finished = true;
    }

    return WRITING_RESPONSE;
}

void process_open_file(const unsigned state, struct selector_key *key){
    //Aca abrimos el archivo y metemos el fd en el selector
    //es de la maquina de estados esto
    pop3* data = GET_POP3(key);
    if(!data->state_data.transaction.file_opened){
        //Tenemos que abrir el archivo y registrarlo en el selector buscando leer
        char * path =  data->path_to_user_maildir;
        int dir_fd = open(path,O_DIRECTORY);//abrimos el directorio
        if(dir_fd==-1){
            //hubo un error abriendo el directorio
            exit(1);
        }
        //Obtenemos el mail que se desea abrir
        email curr_email = data->emails[data->state_data.transaction.arg-1];
        int file_fd = openat(dir_fd,curr_email.name,O_RDONLY);
        if(file_fd==-1){
            exit(1);
        }
        close(dir_fd);//cerramos el directorio, ya no nos sirve
        data->state_data.transaction.file_fd = file_fd; //lo guardamos para ir y volver
        data->state_data.transaction.file_opened = true;
        selector_register(key->s,file_fd,&handler,OP_READ,data);
        (data->references)++; //importante para no liberar si se usa
        return;
    }
    //Nos suscribimos para leer del archivo
    selector_set_interest(key->s,data->state_data.transaction.file_fd,OP_READ);
}
unsigned int process_response(struct  selector_key* key){
    pop3* state = GET_POP3(key);
    if(!state->state_data.transaction.file_opened){
        return FINISHED;//cerramos la conexion, no pudimos abrir el archivo
    }
    //Leer del archivo y mandarlo a el buffer intermedio
    size_t max = 0;
    uint8_t* ptr = buffer_write_ptr(&(state->info_file_buff), &max);
    //Estoy leyendo del archivo, y me deberian llamar aca con key en el archivo
    ssize_t read_count = read(key->fd, ptr, max);
    if(read_count==0){
        //terminamos de leer el archivo, lo señalo para no volver aca
        state->state_data.transaction.file_ended = true;
    }
    if(read_count<0){
        printf("Error leyendo del archivo\n");
        return FINISHED;
    }
    //Avanzamos la escritura en el buffer
    buffer_write_adv(&(state->info_file_buff), read_count);
    if(selector_set_interest(key->s,state->connection_fd, OP_WRITE) != SELECTOR_SUCCESS
        || selector_set_interest(key->s,state->state_data.transaction.file_fd,OP_NOOP)!= SELECTOR_SUCCESS){
        printf("No pude volver a escritura de la respuesta\n");
        return FINISHED;
    }
    if(state->state_data.transaction.file_ended == true){
        //Es la ultima vez que voy a venir al archivo, lo cerramos
        if(selector_unregister_fd(key->s,state->state_data.transaction.file_fd)!= SELECTOR_SUCCESS){
            printf("Error al sacar al archivo del selector");
            return FINISHED;
        }
    }
    //aprovecho que es la misma maquina de estados
    return WRITING_RESPONSE;
}

int dele_action(pop3* state){
    char * msj_ret = ERROR_MESSSAGE;
    //TODO: ver el caso donde no se puede convertir
    long index = strtol(state->arg, NULL,10);
    //REvisamos si se puede eliminar
    if( index < state->emails_count &&  index>=0 &&  !state->emails[index-1].deleted){
        state->emails[index-1].deleted = true;
        msj_ret = OK_MESSSAGE;
    }
    if(try_write(msj_ret, &(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir el mensaje en dele_action y el buffer no tiene espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return  WRITING_RESPONSE;
}



int rset_action(pop3* state){
    //computamos el total de size
    for(int i=0; i<state->emails_count ; i++){
        state->emails[i].deleted = false;
    }
    if(try_write(OK_MESSSAGE, &(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir el mensaje en rset_action y el buffer no tiene espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return  WRITING_RESPONSE;
}
int noop_action(pop3* state){
    if(try_write(OK_MESSSAGE, &(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir el mensaje en noop_action y el buffer no tiene espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return WRITING_RESPONSE;
}



int quit_action(pop3* state){
    if(state->pop3_protocol_state == AUTHORIZATION){
        return FINISHED;//cierro la conexion
    }
    //Estamos en transaction
    //tengo que eliminar todos los archivos que marcaron para eliminar
    int dir_fd = open(state->path_to_user_maildir,O_DIRECTORY);
    if(dir_fd == -1){
        return FINISHED;
    }
    for(size_t i = 0; i<state->emails_count; i++){
        if(state->emails[i].deleted){
            //elimnamos el archivo (cuando ningun proceso lo tenga abierto, lo va a sacar)
            unlinkat(dir_fd,state->emails[i].name,0);
        }
    }
    close(dir_fd);
    //reinicio el estado
    reset_structures(state);
    if(try_write(QUIT_MESSAGE, &(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir el mensaje en noop_action y el buffer no tiene espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return WRITING_RESPONSE;
}

int capa_action(pop3* state){
    if(try_write(CAPA_MESSAGE, &(state->info_write_buff)) == TRY_PENDING){
        printf("Intento escribir el mensaje en noop_action y el buffer no tiene espacio\n");
        return FINISHED;
    }
    state->finished = true;
    return WRITING_RESPONSE;
}
