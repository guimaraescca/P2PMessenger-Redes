#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <generic_deque.h>
#include <contact_list.h>
#include <sys/select.h>

#include <string>

#define SERVER_PORT 28086
#define messageSize 2000

enum threadNames
{
    THREAD_ACCEPTER,
    THREAD_SELECTER,
    THREAD_NUM
};

extern int serverSocket;
extern int threadID[THREAD_NUM];

extern string serverName;

extern fd_set socketSet;

extern ContactList *contacts;
extern ContactList *pendingAccept;

extern int pendingRead;

#endif // _GLOBAL_H_
