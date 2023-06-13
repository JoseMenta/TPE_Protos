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
#include "args.h"
#include "logging/logger.h"

#define MAX_PENDING_CONNECTIONS 20
#define INITIAL_FDS 1024

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}


int main(int argc, const char* argv[]) {
    struct pop3args* pop3_args = malloc(sizeof(struct pop3args));
    if(pop3_args == NULL || errno == ENOMEM){
        return 1;
    }
    parse_args(argc, argv, pop3_args);

    // No queremos que se haga buffering de la salida estandar (que se envíe al recibir un \n), sino que se envíe inmediatamente
    setvbuf(stdout, NULL, _IONBF, 0);
    // Por defecto, el servidor escucha en el puerto que se pasa pero por defecto es el 1100
    unsigned port = pop3_args->pop3_port;

    //No tenemos nada que leer de entrada estandar
    close(STDIN_FILENO);

    //Variables necesarias para usar el selector
    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL; //el selector que usa el servidor

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

    //AF_INET -> indica que usa IPV4
    //SOCK_STREAM -> flujo multidireccional de datos confiable
    //IPPROTO_TCP -> indica que va a usar TCP para lograr lo anterior
    //Devuelve el fd del socket (pasivo)
    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const int server_6 = socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);

    //Si hubo algun error al abrir el socket
    if(server < 0) {
        err_msg = "unable to create socket for ipv4";
        goto finally;
    }

    if(server_6 < 0) {
        err_msg = "unable to create socket for ipv6";
        goto finally;
    }

    printf("pude abrir el socket\n");

    fprintf(stdout, "Listening on TCP port %d\n", port);

    //SOL_SOCKET -> queremos cambiar propiedades del socket
    //SO_REUSEADDR -> deja que otro use el puerto inmediatamente cuando deja de usarlo (sirve por si se reinicia)
    //con el 1, lo mandamos como "habilitado" (recibe un array de opciones)
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    //TODO: revisar, deberia hacer que solo acepte IPV6 y con eso poder usar el mismo puerto
    setsockopt(server_6, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 1 }, sizeof(int));
    setsockopt(server_6, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    //asigna la direccion IP y el puerto al fd server
    //si retorna negativo falla
    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket for ipv4";
        goto finally;
    }

    if(bind(server_6, (struct sockaddr*) &addr_6, sizeof(addr_6)) < 0) {
        err_msg = "unable to bind socket for ipv6";
        goto finally;
    }

    //Marca al socket server como un socket pasivo
    //Si hay mas de MAX_PENDING_CONNECTIONS conexiones en la lista de espera, va a empezar a rechazar algunas 
    if (listen(server, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "unable to listen socket ipv4";
        goto finally;
    }

    if (listen(server_6, MAX_PENDING_CONNECTIONS) < 0) {
        err_msg = "unable to listen socket ipv6";
        goto finally;
    }

    printf("Lista la configuracion del socket\n");


    //Lista la configuracion del socket, ahora pasamos a la configuracion del selector
    
    //Registramos handlers para terminar normalmente en caso de una signal
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);
    


    //agrega el flag de O_NONBLOCK a los flags del server para que usarlo sea no bloqueante 
    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags for ipv4";
        goto finally;
    }

    if(selector_fd_set_nio(server_6) == -1) {
        err_msg = "getting server socket flags for ipv6";
        goto finally;
    }

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
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    printf("selector inicializado\n");

    //usando las configuraciones del init, crea un nuevo selector con capacidad para 1024 FD's inicialmente
    selector = selector_new(INITIAL_FDS);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    //TODO: cambiar para nuestros handlers
    const struct fd_handler pop3_handler = {
        .handle_read       = pop3_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };

    //Registra al fd del server, suscribiendolo para la lectura
    //Como no necesita un dato auxiliar para los handlers, pasa NULL
    ss = selector_register(selector, server, &pop3_handler,
                                              OP_READ, pop3_args);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd for ipv4";
        goto finally;
    }

    ss = selector_register(selector, server_6, &pop3_handler,
                           OP_READ, pop3_args);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd for ipv6";
        goto finally;
    }

    // Null para que solo imprima al archivo, se puede poner stdout para debuggear mas facil
    loggerInit(selector, "", NULL);
    loggerSetLevel(LOG_INFO);
    log(LOG_INFO, "Logging started");

    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            //Hubo un error mientras funcionaba el selector
            err_msg = "serving";
            goto finally;
        }
    }

    printf("cerrando todo\n");

    //Si llegamos hasta aca sin errores, solo hay que decir que termina el servidor
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    //el valor de retorno del programa
    int ret = 0;
    
    finally:
    if(ss != SELECTOR_SUCCESS) { //si terminamos con un error del selector
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) { //si tuvimos un error anterior
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) { //si pudimos obtener el selector, lo liberamos
        selector_destroy(selector);
    }
    selector_close();

    //Si pudimos obtener el socket, lo cerramos
    if(server >= 0) {
        close(server);
    }

    if(server_6 >= 0){
        close(server_6);
    }

    return ret;
}
