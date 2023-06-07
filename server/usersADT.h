#ifndef TP_USERSADT_H
#define TP_USERSADT_H

#define CHUNK 10

typedef struct usersCDT * usersADT;

/*
 * Crea e inicializa la estructura usersADT
 *
 * Retorna la estructura
 */
usersADT usersADT_init();

/*
 * Agrega un usuario
 *
 * Retorna true si pudo agregar el usuario
 * Retorna false si el usuario ya existe
 */
bool usersADT_add(usersADT u, const char * user_name, const char * user_pass);

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
