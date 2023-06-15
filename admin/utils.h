
#ifndef TP_UTILS_H
#define TP_UTILS_H

#define MAX_COMMANDS 50
#define DGRAM_SIZE 1024
#define MAX_LINES 10
#define MAX_LINES_RESP 7
#define OK_TEXT "+"
#define VERSION "1"
#define NAME "PROTOS"

typedef enum{
    ADD_USER,
    CHANGE_PASS,
    REMOVE_USER,
    GET_MAX_MAILS,
    SET_MAX_MAILS,
    GET_MAILDIR,
    SET_MAILDIR,
    STAT_PREVIOUS_CONNECTIONS,
    STAT_CURRENT_CONNECTIONS,
    STAT_BYTES_TRANSFERRED,
}admin_command;


typedef struct command * commands;

struct status_client{
    const char * version;
    const char * name_protocol;
    char ** command_names;
    char list_commands[MAX_COMMANDS][1024];
    int count_commans;
};

typedef struct status_client * client_info;


void version_init( client_info client);

#endif //TP_UTILS_H
