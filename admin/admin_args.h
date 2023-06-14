#ifndef TP_ADMIN_ARGS_H
#define TP_ADMIN_ARGS_H

#define MAX_COMMANDS 50
#define MAX_LINE 128
#define MAX_LINES 10

struct command{
    char text[MAX_LINE*MAX_LINES];
    int size;
};

typedef struct command * commands;

struct status_client{
    const char * version;
    const char * name_protocol;
};

typedef struct status_client * client_info;

void parse_args(const int argc, const char **argv, client_info version);

static void help(const char *progname);

static void printversion(const char * version);

static void showVersion(client_info client, const char * program);

static void user(char *s, char ** user_name, char ** user_pass);

#endif //TP_ADMIN_ARGS_H

