#include <global.h>

// Definição e inicialização das globais.

int serverSocket;
int threadID[THREAD_NUM];

char *serverName = NULL;

fd_set socketSet;
pthread_rwlock_t socketSetSync;

ContactList *contacts = NULL;
ContactList *pendingAccept = NULL;

GenericDeque *pendingRead = NULL;