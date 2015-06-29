#include <contact_list.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <string>
#include <iostream>

int idSeed = 1;

ContactNode *contactNodeCreate( int pSocket, string pName )
{
    ContactNode *newNode = (ContactNode *) malloc( sizeof(ContactNode) );

    newNode->messages = dequeCreate();
    newNode->socket = pSocket;
    newNode->name = (char *) malloc( sizeof(char) * (pName.size() + 1) );
    strcpy( newNode->name, pName.c_str() );
    newNode->prev = newNode->next = NULL;
    newNode->id = idSeed;
    idSeed++;

    return newNode;
}

void contactNodeDestroy( ContactNode *node )
{
    close( node->socket );
    dequeDestroy( node->messages );
    free( node );
}

int contactNodePrint( ContactNode *node )
{
    socklen_t len;
    struct sockaddr_in addr;
    char ipstr[INET_ADDRSTRLEN + 1];

    if ( getpeername( node->socket, (struct sockaddr *)&addr, &len ) != 0 )
    {
        perror( "Erro ao adquirir endereço" );
        return -1;
    }

    inet_ntop( AF_INET, &addr.sin_addr, ipstr, sizeof( ipstr ) );

    cout << "ID: " << node->id << " - Nome: " << node->name << "\nIP: " << ipstr << "\n";
    pthread_rwlock_rdlock( &node->messages->sync );
    if ( node->messages->size > 0 )
        cout << node->messages->size << " mensagens não lidas." << endl;
    pthread_rwlock_unlock( &node->messages->sync );

    return -1;
}

ContactList *contactListCreate()
{
    ContactList *newList = (ContactList *) malloc( sizeof(ContactList) );
    if ( pthread_rwlock_init( &newList->sync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar trava de leitura e escrita da lista" );
        free( newList );
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

    pthread_rwlock_destroy( &list->sync );

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
    list->size++;
}

ContactNode *contactListPopFront( ContactList *list )
{
    ContactNode *front = list->first;

    if ( front != NULL )
    {
        list->first = front->next;
        if ( list->first != NULL )
            list->first->prev = NULL;
        list->size--;
        front->next = NULL;
    }

    return front;
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
    list->size--;
}

ContactNode *contactListSearch( ContactList *list, const char *key )
{
    ContactNode *current = list->first;

    while ( current != NULL && current->name != key )
        current = current->next;

    return current;
}

ContactNode *contactListSearchId( ContactList *list, int key )
{
    ContactNode *current = list->first;

    while ( (current != NULL) && ( current->id != key) )
        current = current->next;

    return current;
}
