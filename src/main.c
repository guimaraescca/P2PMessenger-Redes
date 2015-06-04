#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h> // Contém função close.
#include <sys/select.h> // Para usar select e fd_set.

#include <netdb.h>
#include <arpa/inet.h> // Conversões para formato de rede.

#include <pthread.h>
#include <malloc.h>

#include <contact_list.h>

void menu(){}
void deleteContact(){}
void sendMessage(){}
void broadcastMessage(){}
void pendingAlerts(){}
void exitMenu(){}

int addContact( ContactList *list, fd_set *set )
{
    int errornum, socketDescriptor = socket( PF_INET, SOCK_STREAM, 0 );
    char buffer[81];
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    
    printf( "Digite o endereço IP\t:" );
    scanf( "%15s", buffer );
    
    // Conversão da string em endereço IP.
    if ( (errornum = inet_pton( sa.sin_family, buffer, &sa.sin_addr )) < 1 )
    {
        fprintf( stderr, "Erro ao converter string para endereço: %d\n", errornum );
        return errornum;
    }
    
    // Seleção da porta. Assumindo porta 28086.
    if ( (errornum = (sa.sin_port = htons( 28086 ))) < 1 )
    {
        fprintf( stderr, "Erro ao converter inteiro para porta: %d\n", errornum );
        return errornum;
    }

    int success = 0;
    do 
    {
        printf( "Digite um nome para o contato:\t" );
        scanf( "%80s", buffer );

        if ( ContactListSearch( list, buffer ) != NULL )
            printf( "Nome já utilizado!\n");
        else
            success = 1;
    } while ( success == 0 ) // Enquanto o nome fornecido já tiver sido utilizado.
    
    // Estabelecimento de conexão.
    if ( connect( socketDescriptor, (struct sockaddr *)&sa, sizeof( sa ) ) == -1 )
    {
        perror( "Erro ao estabelecer conexão" );
        return errno;
    }
    
    // TODO: Suspender thread de select para evitar condição de disputa em set.
    FD_SET( socketDescriptor, set );
    // TODO: Reiniciar thread de select.
    contactListInsert( list, ContactNodeCreate( socketDescriptor, buffer ) );
}


void listContact( ContactList *list )
{
    contactNode *current = list->first;
    int i = 1;

    while ( current != NULL )
    {
        printf( "Contato %d\n", i );
        contactNodePrint( current );
        current = current->next;
        ++i;
    }
}

enum threadNames 
{
    THREAD_LISTENER,
    THREAD_ACCEPTER,
    THREAD_SELECTER,
    THREAD_NUM
};

int threadID[THREADNUM]; // Para cancelamento de threads, se necessário.

int main()
{
    //Declaração das threads principais
    pthread_t threads[THREAD_NUM];

    //Assim existam conexões para serem monitoradas
    threadID[THREAD_LISTENER] = pthread_create( &threads[THREAD_LISTENER], NULL, /*LISTEN()*/, NULL);
    threadID[THREAD_ACCEPTER] = pthread_create( &threadID[THREAD_ACCEPTER], NULL, /*ACCEPT()*/, NULL);
    threadID[THREAD_SELECTER] = pthread_create( &threadID[THREAD_SELECTER], NULL, /*SELECT()*/, NULL);
    
}