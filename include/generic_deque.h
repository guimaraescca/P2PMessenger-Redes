#include <pthread.h>

#ifndef _GENERIC_DEQUE_H_
#define _GENERIC_DEQUE_H_

typedef struct st_generic_node
{
    size_t itemSize;
    void *item;

    struct st_generic_node *prev;
    struct st_generic_node *next;
} GenericNode;

typedef struct st_generic_deque
{
    size_t size;
    pthread_rwlock_t sync;

    struct st_generic_node *head;
    struct st_generic_node *tail;
} GenericDeque;

GenericNode *nodeCreate( void *item, size_t pItemSize );

void nodeDestroy( GenericNode *node );

GenericDeque *dequeCreate();

void dequeDestroy( GenericDeque *deque );

void dequePushFront( GenericDeque *deque, GenericNode *newNode );

void dequePushBack( GenericDeque *deque, GenericNode *newNode );

GenericNode *dequePopFront( GenericDeque *deque );

GenericNode *dequePopBack( GenericDeque *deque );

GenericNode *dequeRemove( GenericDeque *deque, GenericNode *node );

#endif // _GENERIC_DEQUE_H_