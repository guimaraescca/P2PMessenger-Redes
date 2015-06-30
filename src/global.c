#include <global.h>

// Definição e inicialização das globais.

int serverSocket;
int threadID[THREAD_NUM];

string serverName;

fd_set socketSet;

ContactList *contacts = NULL;
ContactList *pendingAccept = NULL;

int pendingRead = 0;
