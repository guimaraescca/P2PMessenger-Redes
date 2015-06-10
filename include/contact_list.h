#include <pthread.h>

int idSeed = 100; //Base para gerar os IDs dos contatos.

typedef struct contactNode
{
    char *name;
    int socket;
    int id;
    
    struct contactNode *next;
    struct contactNode *prev;
} ContactNode;

typedef struct contactList
{
    ContactNode *first;
    int size;
    pthread_rwlock_t sync;
} ContactList;

ContactNode *contactNodeCreate( int pSocket, const char *pName );

void contactNodeDestroy( ContactNode *node );

int contactNodePrint( ContactNode *node );

ContactList *contactListCreate();

void contactListDestroy( ContactList *list );

void contactListInsert( ContactList *list, ContactNode *node );

void contactListRemove( ContactList *list, ContactNode *node );

ContactNode *contactListSearch( ContactList *list, const char *key );
