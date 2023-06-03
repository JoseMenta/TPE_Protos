#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <stdlib.h>
#include "selector.h"
#include "pop3.h"

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
    if(selector_register(key->s,client_fd,&handler,OP_WRITE,state)!= SELECTOR_SUCCESS){
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


    return ans;
}

//borrar y liberar el pop3
void pop3_destroy(pop3* state){
    if(state == NULL){
        return;
    }

    //Liberamos la estructura para el estado
    free(state);
}

void pop3_read(struct selector_key* key){
    
}

