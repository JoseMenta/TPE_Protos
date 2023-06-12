#ifndef __POP3_H__
#define __POP3_H__

typedef struct pop3 pop3;

#define GET_POP3(key) ((pop3*) (key)->data)
#define BUFFER_SIZE 1024


void pop3_read(struct selector_key* key);
void pop3_write(struct selector_key* key);
void pop3_passive_accept(struct selector_key* key);
void pop3_close(struct selector_key* key);

#endif
