#include "maidir_reader.h"
#include <sys/types.h>   // socket, opendir
#include <sys/stat.h> //stat
#include <dirent.h> //readdir
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "logging/logger.h"


#define CHUNK_SIZE 10

email* read_maildir(const char* maildir_path, size_t* size){
    size_t i = 0;
    size_t ans_size = CHUNK_SIZE;
    email* ans = malloc(CHUNK_SIZE * sizeof (email));
    DIR* mail_dir = NULL;
    if(ans == NULL || errno == ENOMEM){
        log(LOG_FATAL, "Error to allocate memory for emails");
        goto fail;
    }
    if(maildir_path == NULL){
        log(LOG_FATAL, "Maildir_path is null");
        goto fail;
    }
    mail_dir = opendir(maildir_path);
    if(mail_dir == NULL){
        log(LOG_FATAL, "An error occurred opening maildir_path");
        goto fail;
    }
    struct dirent* dirent = NULL;
    while(dirent = readdir(mail_dir),dirent != NULL && i<*size){
        if(strcmp(dirent->d_name,".")!=0 && strcmp(dirent->d_name,"..")!=0){
            //Tengo que considerar al directorio
            struct stat file_stat;
            //TODO: probar esto, me ahorra hacer la concatenacion para tener todos los paths
            if(fstatat(dirfd(mail_dir),dirent->d_name,&file_stat,0)==-1){
                log(LOG_ERROR, "An error occurred when using fstatat");
                goto fail;
            }
            if(S_ISREG(file_stat.st_mode)){
                //Es un archivo regular, lo considero como un mail
                if(ans == NULL || i>=ans_size){
                    void* aux =  realloc(ans,(ans_size+CHUNK_SIZE)*sizeof (email));
                    if(aux==NULL || errno == ENOMEM){
                        log(LOG_ERROR, "Error when using realloc for normal file");
                        ans = aux; //tengo que liberar la memoria original
                        goto fail;
                    }
                    ans = aux;
                    ans_size+=CHUNK_SIZE;
                }
                ans[i].size = file_stat.st_size;
                ans[i].deleted = false;
                strncpy(ans[i].name,dirent->d_name,NAME_SIZE);
                i++;
            }

        }
    }
    closedir(mail_dir);
    *size = i;
    return ans;
    fail:
    if(mail_dir!=NULL){
        closedir(mail_dir);
    }
    if(ans != NULL){
        free(ans);
    }
    return NULL;
}
void free_emails(email* emails, size_t size){
    if(emails == NULL){
        log(LOG_DEBUG, "Trying to free emails but it was null");
        return;
    }
    free(emails);
}
