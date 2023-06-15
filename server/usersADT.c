#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "usersADT.h"
#include "logging/logger.h"

static int usersADT_find_user(usersADT u, const char * user_name);

usersADT usersADT_init(void){
    log(LOG_INFO, "Initializing usersADT");
    usersADT u = calloc(1, sizeof(struct usersCDT));
    if(u == NULL || errno == ENOMEM) {
        log(LOG_FATAL, "Unable to allocate memory for usersADT");
        return NULL;
    }
    u->array_length = CHUNK;
    u->users_count = 0;
    u->users_array = calloc(CHUNK, sizeof(user_t));
    if(u->users_array == NULL || errno == ENOMEM){
        log(LOG_FATAL, "Unable to allocate memory for users_array");
        free(u);
        return NULL;
    }
    return u;
}

void usersADT_destroy(usersADT u) {
    log(LOG_INFO, "Destroying usersADT");
    for(unsigned int i = 0; i < u->users_count; i++) {
        free((char *) u->users_array[u->users_count].name);
        free(u->users_array[u->users_count].pass);
    }
    free(u->users_array);
    free(u);
}

int usersADT_add(usersADT u, const char * user_name, const char * user_pass) {
    if(usersADT_find_user(u,user_name) != -1){
        logf(LOG_ERROR, "User '%s' already in ADT", user_name);
        return -1;
    }
    char * name = NULL;
    char * pass = NULL;
    unsigned int name_length = strlen(user_name);
    name = calloc(name_length + 1, sizeof(char));
    if(name == NULL || errno == ENOMEM) {
        logf(LOG_ERROR, "Unable to allocate memory for name of user '%s'", user_name);
        goto error;
    }
    strncpy(name, user_name, name_length);
    unsigned int pass_length = strlen(user_pass);
    pass = calloc(pass_length + 1, sizeof(char));
    if(pass == NULL  || errno == ENOMEM) {
        logf(LOG_ERROR, "Unable to allocate memory for pass of user '%s'", user_name);
        goto error;
    }
    strncpy(pass, user_pass, pass_length);
    if(u->users_count == u->array_length){
        user_t * aux = realloc(u->users_array, sizeof(user_t)*(u->array_length + CHUNK));
        if(aux == NULL) {
            logf(LOG_FATAL, "Unable to reallocate memory for usersADT, current size: %d", u->array_length);
            goto error;
        }
        u->users_array = aux;
        u->array_length += CHUNK;
    }
    u->users_array[u->users_count].name = name;
    u->users_array[u->users_count].pass = pass;
    u->users_array[u->users_count].logged = false;
    u->users_count++;
    return 0;

    error:
        if(name != NULL) {
            free(name);
        }
        if(pass != NULL) {
            free(pass);
        }
        return -2;
}

char * usersADT_get_user_mail_path(usersADT u, const char * base_path, const char * user_name) {
    int user_index = usersADT_find_user(u, user_name);
    if(user_index == -1) {
        logf(LOG_ERROR, "Cannot find user '%s' to get mail path", user_name);
        return NULL;
    }
    unsigned int base_path_len = strlen(base_path);
    unsigned int user_name_len = strlen(user_name);
    unsigned int curl_len = strlen(CURL_PATH);
    char * user_mail_path = calloc(base_path_len + user_name_len + curl_len + 2, sizeof(char));
    if(user_mail_path == NULL || errno == ENOMEM) {
        log(LOG_ERROR,"Error allocating memory for user mail path");
        return NULL;
    }
    strncpy(user_mail_path, base_path, base_path_len);
//    user_mail_path[base_path_len] = '/';
    strncat(user_mail_path, user_name, user_name_len);
    strncat(user_mail_path, CURL_PATH, curl_len);
    return user_mail_path;
}

bool usersADT_validate(usersADT u, const char * user_name, const char * user_pass) {
    int user_index = usersADT_find_user(u, user_name);
    if(user_index == -1){
        logf(LOG_ERROR, "Cannot find user '%s' to validate", user_name);
        return false;
    }
    return strcmp(u->users_array[user_index].pass, user_pass);
}

bool usersADT_update_pass(usersADT u, const char * user_name, const char * new_pass){
    for(unsigned int i = 0; i < u->users_count; i++){
        if(strcmp(u->users_array[i].name, user_name) == 0){
            logf(LOG_DEBUG, "Updating pass for user '%s'", user_name);
            //TODO: esto es un error, al principio las copia pero ahora solo guarda el puntero
            free(u->users_array[i].pass);
            unsigned int pass_length = strlen(new_pass);
            char* pass = calloc(pass_length + 1, sizeof(char));
            if(pass == NULL  || errno == ENOMEM) {
                return false;
            }
            strncpy(pass, new_pass, pass_length);
            u->users_array[i].pass = pass;
            //OJO con esto, puede pasarse si la contraseña es mas larga
            return true;
        }
    }
    return false;
}

static int usersADT_find_user(usersADT u, const char * user_name) {
    for(unsigned int i = 0; i < u->users_count; i++) {
        if(strcmp(u->users_array[i].name, user_name) == 0) {
            return i;
        }
    }
    return -1;
}
