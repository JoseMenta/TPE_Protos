#ifndef TP_USERSADT_H
#define TP_USERSADT_H

#include <stdbool.h>

#define CHUNK 10
#define CURL_PATH "/cur"

typedef struct{
    char *name;
    char *pass;
} user_t;

struct usersCDT {
    user_t * users_array;
    unsigned int array_length;
    unsigned int users_count;
};

typedef struct usersCDT * usersADT;

/*
 * Crea e inicializa la estructura usersADT
 *
 * Retorna la estructura
 */
usersADT usersADT_init();

/*
 * Libera los recursos utilizados en la estructura
 */
void usersADT_destroy(usersADT u);

/*
 * Agrega un usuario
 *
 * Retorna 0 si pudo agregar el usuario
 * Retorna -1 si el usuario ya existe
 * Retorna -2 si hubo problemas para agregar el usuario
 */
int usersADT_add(usersADT u, const char * user_name, const char * user_pass);

/*
 * Dado el basepath del directorio, devuelve el path al Maildir del usuario
 *
 * En caso de error, devuelve NULL
 */
char * usersADT_get_user_mail_path(usersADT u, const char * base_path, const char * user_name);

/*
 * Verifica que el usuario exista y las credenciales sean validas
 *
 * Devuelve true si logro validarlo
 * Devuelve false si falló
 */
bool usersADT_validate(usersADT u, const char * user_name, const char * user_pass);

/*
 * Actualiza la contraseña de user_name con new_pass
 *
 * Devuelve true si pudo actualizarlo
 * Devuelve false si no existe el usuario
 */
bool usersADT_update_pass(usersADT u, const char * user_name, const char * new_pass);


#endif //TP_USERSADT_H
