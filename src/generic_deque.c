#include <generic_deque.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GenericNode *nodeCreate( void *item, size_t pItemSize )
{
    assert( item != NULL );

    GenericNode *newNode = (GenericNode *) malloc( sizeof( GenericNode ) );

    newNode->item = malloc( pItemSize );
    memcpy( newNode->item, item, pItemSize );
    newNode->itemSize = pItemSize;
    newNode->prev = newNode->next = NULL;

    return newNode;
}

void nodeDestroy( GenericNode *node )
{
    free( node->item );
    free( node );
}

GenericDeque *dequeCreate()
{
    GenericDeque *newDeque = (GenericDeque *) malloc( sizeof( GenericDeque ) );

    if ( pthread_rwlock_init( &newDeque->sync, NULL ) != 0 ) {
        perror( "Erro ao inicializar trava de leitura e escrita do Deque" );
        free( newDeque );
        return NULL;
    }

    newDeque->size = 0;
    newDeque->head = newDeque->tail = NULL;

    return newDeque;
}

void dequeDestroy( GenericDeque *deque )
{
    GenericNode *current = deque->head, *next;

    while ( current != NULL ) {
        next = current->next;
        nodeDestroy( current );
        current = next;
    }

    free( deque );
}

void dequePushFront( GenericDeque *deque, GenericNode *newNode )
{
    // pthread_rwlock_wrlock( &deque->sync );

    if( deque->head != NULL ) {
        deque->head->prev = newNode;
        newNode->next = deque->head;
    }
    else
        deque->tail = newNode;

    deque->head = newNode;
    deque->size++;

    // pthread_rwlock_unlock( &deque->sync );
}

void dequePushBack( GenericDeque *deque, GenericNode *newNode )
{
    // pthread_rwlock_wrlock( &deque->sync );

    if( deque->tail != NULL ) {
        deque->tail->next = newNode;
        newNode->prev = deque->tail;
    }
    else
        deque->head = newNode;

    deque->tail = newNode;
    deque->size++;

    // pthread_rwlock_unlock( &deque->sync );
}

GenericNode *dequePopFront( GenericDeque *deque )
{
    // pthread_rwlock_wrlock( &deque->sync );

    GenericNode *removed = deque->head;

    if ( removed != NULL ) {
        if ( deque->head == deque->tail )
            deque->tail = NULL;

        deque->head = deque->head->next;
        removed->next = removed->prev = deque->head->prev = NULL;
        deque->size--;
    }

    // pthread_rwlock_unlock( &deque->sync );

    return removed;
}

GenericNode *dequePopBack( GenericDeque *deque )
{
    // pthread_rwlock_wrlock( &deque->sync );

    GenericNode *removed = deque->tail;

    if ( removed != NULL ) {
        if ( deque->tail == deque->head )
            deque->head = NULL;

        deque->tail = deque->tail->prev;
        removed->next = removed->prev = deque->tail->next = NULL;
        deque->size--;
    }

    // pthread_rwlock_unlock( &deque->sync );

    return removed;
}

GenericNode *dequeRemove( GenericDeque *deque, GenericNode *node )
{
    // pthread_rwlock_wrlock( &deque->sync );

    if ( node->next != NULL )
        node->next->prev = node->prev;
    else {
        deque->tail = node->prev;
        deque->tail->next = NULL;
    }
    if ( node->prev != NULL )
        node->prev->next = node->next;
    else {
        deque->head = node->next;
        deque->head->prev = NULL;
    }

    node->next = node->prev = NULL;
    deque->size--;

    // pthread_rwlock_unlock( &deque->sync );

    return node;
}
