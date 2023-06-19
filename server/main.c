#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "selector.h"
#include "pop3.h"
#include "admin.h"
#include "args.h"
#include "logging/logger.h"

#define MAX_PENDING_CONNECTIONS 20
#define INITIAL_FDS 1024

static bool done = false;

static void
sigterm_handler(const int signal) {
    logf(LOG_INFO, "Raised signal: %d", signal);
    done = true;
}


int main(int argc, const char* argv[]) {
    //el valor de retorno del programa
    int ret = 0;

    //Variables necesarias para usar el selector
    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL; //el selector que usa el servidor

    //Opciones de configuracion del selector
    //Decimos que usamos sigalarm para los trabajos bloqueantes (no nos interesa)
    //Espera 10 segundos hasta salir del select interno
    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };

    //Guarda las configuraciones para el selector
    if(0 != (ss = selector_init(&conf))) {
        fprintf(stderr, "Cannot initialize selector: '%s'\n", ss == SELECTOR_IO ? strerror(errno) : selector_error(ss));
        return 2;
    }

    //usando las configuraciones del init, crea un nuevo selector con capacidad para 1024 FD's inicialmente
    selector = selector_new(INITIAL_FDS);
    if(selector == NULL) {
        fprintf(stderr, "Unable to create selector: '%s'\n", strerror(errno));
        return 1;
    }

    if(logger_init(selector, "", NULL)!=0){
        fprintf(stderr,"Unable to initialize logger\n");
        return 1;
    }

    struct pop3args* pop3_args = malloc(sizeof(struct pop3args));
    if(pop3_args == NULL || errno == ENOMEM){
        log(LOG_FATAL, "Cant allocate memory for pop3args");
        return 1;
    }
    parse_args(argc, argv, pop3_args);

    logger_set_level(pop3_args->log_level);
    log(LOG_INFO, "Initializing logger");

    // No queremos que se haga buffering de la salida estandar (que se envíe al recibir un \n), sino que se envíe inmediatamente
    log(LOG_DEBUG, "Setting stdout to unbuffered");
    setvbuf(stdout, NULL, _IONBF, 0);
    // Por defecto, el servidor escucha en el puerto que se pasa pero por defecto es el 1100
    unsigned port = pop3_args->pop3_port;
    fprintf(stdout, "Listening on port %d\n", port);

    //No tenemos nada que leer de entrada estandar
    log(LOG_DEBUG, "Closing STDIN fd");
    close(STDIN_FILENO);

    //Address para hacer el bind del socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;             // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);   // Todas las interfaces (escucha por cualquier IP)
    addr.sin_port        = htons(port);         // Server port

    struct sockaddr_in6 addr_6;
    memset(&addr_6, 0, sizeof(addr_6));
    addr_6.sin6_family = AF_INET6;
    addr_6.sin6_addr = in6addr_any;
    addr_6.sin6_port = htons(port);

    struct sockaddr_in admin_addr;
    memset(&admin_addr,0,sizeof (admin_addr));
    admin_addr.sin_family = AF_INET;
    admin_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    admin_addr.sin_port = htons(1024);

    log(LOG_DEBUG, "Opening POP3 IPv4 and IPv6 sockets");
    //AF_INET -> indica que usa IPV4
    //SOCK_STREAM -> flujo multidireccional de datos confiable
    //IPPROTO_TCP -> indica que va a usar TCP para lograr lo anterior
    //Devuelve el fd del socket (pasivo)
    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const int server_6 = socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);

    log(LOG_DEBUG, "Opening ADMIN socket");
    const int admin = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //Si hubo algun error al abrir el socket
    if(server < 0) {
        err_msg = "Unable to create socket for IPv4";
        goto finally;
    }

    if(server_6 < 0) {
        err_msg = "Unable to create socket for IPv6";
        goto finally;
    }

    if(admin < 0){
        err_msg = "Unable to create socket for admin in ipv4";
        goto finally;
    }

    logf(LOG_DEBUG, "Listening on TCP port %d", port);

    //SOL_SOCKET -> queremos cambiar propiedades del socket
    //SO_REUSEADDR -> deja que otro use el puerto inmediatamente cuando deja de usarlo (sirve por si se reinicia)
    //con el 1, lo mandamos como "habilitado" (recibe un array de opciones)
    log(LOG_DEBUG, "Setting SO_REUSEADDR on IPv4 socket");
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    log(LOG_DEBUG, "Setting IPV6_V6ONLY and SO_REUSEADDR on IPv6 socket");
    setsockopt(server_6, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof(int));
    setsockopt(server_6, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    log(LOG_DEBUG, "Setting SO_REUSEADDR on IPv4 socket");
    setsockopt(admin, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    //asigna la direccion IP y el puerto al fd server
    //si retorna negativo falla
    log(LOG_INFO, "Binding socket for IPv4");
    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "Unable to bind socket for IPv4";
        goto finally;
    }

    log(LOG_INFO, "Binding socket for IPv6");
    if(bind(server_6, (struct sockaddr*) &addr_6, sizeof(addr_6)) < 0) {
        err_msg = "Unable to bind socket for IPv6";
        goto finally;
    }

    log(LOG_INFO, "Binding socket for ADMIN");
    if(bind(admin, (struct sockaddr*) &admin_addr, sizeof(admin_addr)) < 0) {
        err_msg = "unable to bind socket for admin in ipv4";
        goto finally;
    }

    //Marca al socket server como un socket pasivo
    //Si hay mas de MAX_PENDING_CONNECTIONS conexiones en la lista de espera, va a empezar a rechazar algunas
    log(LOG_INFO, "Start listening for incoming connections for IPv4 socket");
    if (listen(server, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen in IPv4 socket";
        goto finally;
    }

    log(LOG_INFO, "Start listening for incoming connections for IPv6 socket");
    if (listen(server_6, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "Unable to listen in IPv6 socket";
        goto finally;
    }


    //Lista la configuracion del socket, ahora pasamos a la configuracion del selector
    
    //Registramos handlers para terminar normalmente en caso de una signal
    log(LOG_DEBUG, "Registering signal handlers for SIGTERM and SIGINT");
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);
    

    log(LOG_INFO, "Setting IPv4 socket as non-blocking");
    //agrega el flag de O_NONBLOCK a los flags del server para que usarlo sea no bloqueante 
    if(selector_fd_set_nio(server) == -1) {
        err_msg = "Unable to set IPv4 socket as non-blocking";
        goto finally;
    }

    log(LOG_INFO, "Setting IPv6 socket as non-blocking");
    if(selector_fd_set_nio(server_6) == -1) {
        err_msg = "Unable to set IPv6 socket as non-blocking";
        goto finally;
    }

    log(LOG_INFO, "Setting ADMIN socket as non-blocking");
    if(selector_fd_set_nio(admin) == -1){
        err_msg = "getting server socket flags for admin in ipv4";
        goto finally;
    }


    const struct fd_handler pop3_handler = {
            .handle_read       = pop3_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };

    const struct fd_handler admin_handler = {
            .handle_read    = admin_read,
            .handle_write   = NULL,
            .handle_close   = NULL
    };

    log(LOG_INFO, "Setting IPv4 socket as passive");
    //Registra al fd del server, suscribiendolo para la lectura
    //Como no necesita un dato auxiliar para los handlers, pasa NULL
    ss = selector_register(selector, server, &pop3_handler,
                                              OP_READ, pop3_args);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register fd for IPv4 socket";
        goto finally;
    }

    log(LOG_INFO, "Setting IPv6 socket as passive");
    ss = selector_register(selector, server_6, &pop3_handler,
                           OP_READ, pop3_args);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "Unable to register fd for IPv6 socket";
        goto finally;
    }

    log(LOG_INFO, "Setting ADMIN UDP socket");
    ss = selector_register(selector, admin, &admin_handler,
                           OP_READ, pop3_args);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd for admin in ipv4";
        goto finally;
    }

    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            //Hubo un error mientras funcionaba el selector
            err_msg = "An error occurred while selecting";
            goto finally;
        }
    }

    log(LOG_INFO, "Closing everything");

    //Si llegamos hasta aca sin errores, solo hay que decir que termina el servidor
    if(err_msg == NULL) {
        log(LOG_INFO, "Server terminated normally");
    }
    
    finally:
    if(ss != SELECTOR_SUCCESS) { //si terminamos con un error del selector
        logf(LOG_FATAL, "'%s': '%s'", (err_msg == NULL) ? "": err_msg,
             ss == SELECTOR_IO
             ? strerror(errno)
             : selector_error(ss));

        ret = 2;
    } else if(err_msg) { //si tuvimos un error anterior
        logf(LOG_FATAL, "An error occurred: '%s'", err_msg);
        ret = 1;
    }
    if(selector != NULL) { //si pudimos obtener el selector, lo liberamos
        log(LOG_INFO, "Destroying selector");
        selector_destroy(selector);
    }
    log(LOG_INFO, "Closing selector");
    selector_close();
    usersADT_destroy(pop3_args->users);
    free(pop3_args->maildir_path);
    free(pop3_args);

    //Si pudimos obtener el socket, lo cerramos
    if(server >= 0) {
        log(LOG_INFO, "Closing IPv4 socket");
        close(server);
    }

    if(server_6 >= 0){
        log(LOG_INFO, "Closing IPv6 socket");
        close(server_6);
    }

    if(admin >=0){
        log(LOG_INFO,"Closing ADMIN socket");
        close(admin);
    }

    logf(LOG_INFO, "Server terminated with code %d", ret);
    logger_finalize();
    return ret;
}
