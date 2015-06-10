#include <contact_list.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>

ContactNode *contactNodeCreate( int pSocket, const char *pName )
{
    ContactNode *newNode = (ContactNode *) malloc( sizeof(ContactNode) );
    newNode->socket = pSocket;
    strcpy( newNode->name, pName );
    newNode->next = NULL;
    newNode->prev = NULL;
    newNode->id = idSeed;
    idSeed++;

    return newNode;
}

void contactNodeDestroy( ContactNode *node )
{
    close( node->socket );
    free( node->name );
    free( node );
}

int contactNodePrint( ContactNode *node )
{
    socklen_t len;
    struct sockaddr_in addr;
    char ipstr[INET_ADDRSTRLEN];

    if ( getpeername( node->socket, (struct sockaddr *)&addr, &len ) != 0 )
    {
        perror( "Erro ao adquirir endereço" );
        return errno;
    }

    inet_ntop( AF_INET, &addr.sin_addr, ipstr, sizeof( ipstr ) );

    printf( "ID: %d\nNome: %s\nIP: %s\n", node->id, node->name, ipstr );

    return -1;
}

ContactList *contactListCreate()
{
    ContactList *newList = (ContactList *) malloc( sizeof(ContactNode) );
    if ( pthread_rwlock_init( &newList->sync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar trava de leitura e escrita da lista" );
        return NULL;
    }
    newList->first = NULL;
    newList->size = 0;

    return newList;
}

void contactListDestroy( ContactList *list )
{
    ContactNode *current = list->first, *next;

    while ( current != NULL )
    {
        next = current->next;
        contactNodeDestroy( current );
        current = next;
    }

    free( list );
}

void contactListInsert( ContactList *list, ContactNode *node )
{
    pthread_rwlock_wrlock( &list->sync );

    if ( list->first != NULL )
    {
        node->next = list->first;
        list->first->prev = node;
    }
    list->first = node;
    list->size++;

    pthread_rwlock_unlock( &list->sync );
}

void contactListRemove( ContactList *list, ContactNode *node )
{
    pthread_rwlock_wrlock( &list->sync );

    if ( list->first == node )
        list->first = NULL;
    else
    {
        if ( node->next != NULL )
            node->next->prev = node->prev;
        node->prev->next = node->next;
    }
    contactNodeDestroy( node );
    list->size--;

    pthread_rwlock_unlock( &list->sync );
}

ContactNode *contactListSearch( ContactList *list, const char *key )
{
    pthread_rwlock_rdlock( &list->sync );

    ContactNode *current = list->first;

    while ( current != NULL && (strcmp( key, current->name ) != 0) )
        current = current->next;

    return current;
}

ContactNode *contactListSearchId( ContactList *list, int key )
{
    pthread_rwlock_rdlock( &list->sync );

    ContactNode *current = list->first;

    while ( (current != NULL) && ( current->id != key) )
        current = current->next;

    return current;
}