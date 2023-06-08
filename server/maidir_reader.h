#ifndef TPE_PROTOS_MAIDIR_READER_H
#define TPE_PROTOS_MAIDIR_READER_H
#include <sys/types.h>
#include <stdbool.h>
#define NAME_SIZE 256

typedef struct email email;

struct email{
    char name[NAME_SIZE]; //to open the file later, use the name limit of readdir
    off_t size; //es un int
    bool deleted;
};

/*
 * Returns a dynamic array with entries for each directory file
 * size: the max size for the array, it returns the actual size
 */
email* read_maildir(const char* maildir_path, size_t* size);

void free_emails(email* emails, size_t size);

#endif //TPE_PROTOS_MAIDIR_READER_H
