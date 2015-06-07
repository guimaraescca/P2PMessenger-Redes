typedef struct contactNode
{
    char *name;
    int socket;

    struct contactNode *next;
    struct contactNode *prev;
} ContactNode;

typedef struct contactList
{
    ContactNode *first;
    pthread_rwlock_t sync;
} ContactList;

ContactNode *contactNodeCreate( int pSocket, const char *pName );

void contactNodeDestroy( ContactNode *node );

ContactList *contactListCreate();

void contactListDestroy( ContactList *list );

void contactListInsert( ContactList *list, ContactNode *node );

void contactListRemove( ContactList *list, ContactNode *node );

ContactNode *contactListSearch( ContactList *list, const char *key );