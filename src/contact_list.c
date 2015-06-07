#include <contact_list.h>

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <pthread.h>

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
    if ( pthread_rwlock_init( &newList->sync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar trava de leitura e escrita da lista" );
        return NULL;
    }
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
    pthread_rwlock_wrlock( &list->sync );

    if ( list->first != NULL ) 
    {
        node->next = list->first;
        list->first->prev = node;
    }
    list->first = node;

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

    pthread_rwlock_unlock( &list->sync );
}

ContactNode *contactListSearch( ContactList *list, const char *key )
{
    pthread_rwlock_rdlock( &list->sync );

    ContactNode *current = list->first;

    while ( current != NULL && (strcmp( key, current->name ) != 0) )
        current = current->next;

    pthread_rwlock_unlock( &list->sync );

    /* TODO: A partir desse ponto o endereço apontado por current pode ser 
        alterado por outra thread. Escolher solução:
        1 - Após contactListSearch ser usada, o usuário deve explicitamente
        destravar a trava. (Eu acho a melhor)
        2 - Associar a cada nó uma trava de leitura-escrita.
        3 - Manter como está, só daria conflito se o nó for deletado ao mesmo
        tempo que outra thread tentar usá-lo, como a exclusão e o envio de
        mensagens são as únicas funcionalidades que deletam nós, é "só" garantir
        que elas não vão ocorrer em paralelo a outras que acessam valores nos
        nós.
    */

    return current;
}