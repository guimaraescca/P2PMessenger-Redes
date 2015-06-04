#include <contact_list.h>

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>

ContactNode *contactNodeCreate( int pSocket, const char *pName )
{
    ContactNode *newNode = (ContactNode *) malloc( sizeof(ContactNode) );
    newNode->socket = pSocket;
    strcpy( newNode->name, pName );
    newNode->next = NULL;
    newNode->prev = NULL;

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
    
    inet_ntop( AF_INET, &addr->sin_addr, ipstr, sizeof( ipstr ) );
    
    printf( "Nome: %s\nIP: %s\n", node->name, ipstr );
}

ContactList *contactListCreate()
{
    ContactList *newList = (ContactList *) malloc( sizeof(ContactNode) );
    newList->first = NULL;

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
    if ( list->first != NULL ) 
    {
        node->next = list->first;
        list->first->prev = node;
    }
    list->first = node;
}

void contactListRemove( ContactList *list, ContactNode *node )
{
    if ( list->first == node )
        list->first = NULL;
    else 
    {
        if ( node->next != NULL )
            node->next->prev = node->prev;
        node->prev->next = node->next;
    }
    contactNodeDestroy( node );
}

ContactNode *contactListSearch( ContactList *list, const char *key )
{
    ContactNode *current = list->first;
    
    while ( current != NULL && strcmp( key, current->name ) != 0 )
        current = current->next;

    return current;
}