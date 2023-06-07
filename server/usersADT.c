#include <stdlib.h>
#include <string.h>
#include "usersADT.h"

typedef struct{
    const char *name;
    char *pass;
} user_t;

struct usersCDT {
    user_t * users_array;
    unsigned int array_length;
    unsigned int users_count;
};

static int usersADT_find_user(usersADT u, const char * user_name);

usersADT usersADT_init(){
    usersADT u = calloc(1, sizeof(struct usersCDT));
    if(u == NULL) {
        return u;
    }
    u->array_length = CHUNK;
    u->users_count = 0;
    u->users_array = calloc(CHUNK, sizeof(user_t));
    if(u->users_array == NULL){
        free(u);
        return NULL;
    }
    return u;
}

int usersADT_add(usersADT u, const char * user_name, const char * user_pass) {
    if(usersADT_find_user(u,user_name) == -1){
        return -1;
    }
    user_t user = NULL;
    char * name = NULL;
    char * pass = NULL;
    user = calloc(1, sizeof(user_t));
    if(user == NULL) {
        goto error;
    }
    int name_length = strlen(user_name);
    name = calloc(name_length + 1, sizeof(char));
    if(name == NUL)
    strncpy(name, user_name, name_length);
    user.pass = (char *) user_pass;
    user.name = user_name;
    u->users_count++;
    if(u->users_count == u->array_length){
        user_t * aux = realloc(u->users_array, sizeof(user_t)*(u->array_length + CHUNK));
        if(aux == NULL) {
            return -2;
        }
        u->users_array = aux;
        u->array_length += CHUNK;
    }
    u->users_array[u->users_count] = user;
    return 0;

    error:
        if(user != NULL) {
            free(user);
        }
        if(name != NULL) {
            free(name);
        }
        if(pass != NULL) {
            free(pass);
        }
        return -2;
}

char * usersADT_get_user_mail_path(usersADT u, const char * base_path, const char * user_name) {
    int user_index = usersADT_user_exists(u, user_name);
    if(user_index) {
        return -1;
    }
    unsigned int base_path_len = strlen(base_path);
    unsigned int user_name_len = strlen(user_name);
    char * user_mail_path = calloc(base_path_len + user_name_len + 2, sizeof(char));
    if(user_mail_path == NULL) {
        return NULL;
    }
    strncpy(user_mail_path, base_path, base_path_len);
    user_mail_path[base_path_len] = '/';
    strncat(user_mail_path, user_name, user_name_len);
    return user_mail_path;
}

bool usersADT_validate(usersADT u, const char * user_name, const char * user_pass) {
    int user_index = usersADT_user_exists(u, user_name);
    if(user_index == -1){
        return false;
    }
    return strcmp(u->users_array[user_index].pass, user_pass);
}

bool usersADT_update_pass(usersADT u, const char * user_name, const char * new_pass){
    for(unsigned int i = 0; i < u->users_count; i++){
        if(strcmp(u->users_array[i].name,user)){
            strcpy(u->users_array[i].pass, new_pass);
            return true;
        }
    }
    return false;
}

static int usersADT_find_user(usersADT u, const char * user_name) {
    for(unsigned int i = 0; i < u->users_count; i++) {
        if(strcmp(u->users_array[i].name, user_name)) {
            return i;
        }
    }
    return -1;
}
