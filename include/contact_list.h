#include <pthread.h>
#include <generic_deque.h>

#ifndef _CONTACT_LIST_H_
#define _CONTACT_LIST_H_

int idSeed; //Base para gerar os IDs dos contatos.

typedef struct contactNode
{
    char *name;
    int socket;
    int id;

    GenericDeque *messages;

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

ContactNode *contactListPopFront( ContactList *list );


/* As funções de busca devem ser usadas com cuidado, já que podem levar 
* a condições de disputa ou impasses.
*/
void contactListRemove( ContactList *list, ContactNode *node );

ContactNode *contactListSearch( ContactList *list, const char *key );

ContactNode *contactListSearchId( ContactList *list, int key );

#endif // _CONTACT_LIST_H_