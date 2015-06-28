#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <generic_deque.h>
#include <contact_list.h>
#include <sys/select.h>

#define SERVER_PORT 28086
#define messageSize 2000

// Definição de globais INICIO
enum threadNames
{
    THREAD_ACCEPTER,
    THREAD_SELECTER,
    THREAD_NUM
};

int serverSocket;
int threadID[THREAD_NUM];

char *serverName = NULL;

fd_set socketSet;
pthread_rwlock_t socketSetSync;

ContactList *contacts = NULL;
ContactList *pendingAccept = NULL;

GenericDeque *pendingRead = NULL;

#endif // _GLOBAL_H_