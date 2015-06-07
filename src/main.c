#include <errno.h>
#include <locale.h>
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

#include <contact_list.h>

#define SERVER_PORT 28086

// Definição de globais INICIO
enum threadNames
{
    THREAD_LISTENER,
    THREAD_ACCEPTER,
    THREAD_SELECTER,
    THREAD_NUM
};

int threadID[THREAD_NUM];

ContactList *contacts = NULL;

pthread_rwlock_t socketSetSync;
fd_set socketSet; // Deve ser inicializado.

// Definição de globais FIM

void menu(){}
void sendMessage(){}
void broadcastMessage(){}
void pendingAlerts(){}
void exitMenu(){}

int addContact()
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

    // Seleção da porta.
    if ( (errornum = (sa.sin_port = htons( SERVER_PORT ))) < 1 )
    {
        fprintf( stderr, "Erro ao converter inteiro para porta: %d\n", errornum );
        return errornum;
    }

    int notUsed = 0;
    do
    {
        printf( "Digite um nome para o contato:\t" );
        scanf( "%80s", buffer );

        if ( contactListSearch( contacts, buffer ) != NULL )
            printf( "Nome já utilizado!\n");
        else
            notUsed = 1;
    } while ( notUsed == 0 ); // Enquanto o nome fornecido já tiver sido utilizado.

    // Estabelecimento de conexão.
    if ( connect( socketDescriptor, (struct sockaddr *)&sa, sizeof( sa ) ) == -1 )
    {
        perror( "Erro ao estabelecer conexão" );
        return errno;
    }

    // Atualizar conjunto de sockets.
    pthread_rwlock_wrlock( &socketSetSync );
    FD_SET( socketDescriptor, &socketSet );
    pthread_rwlock_unlock( &socketSetSync );

    contactListInsert( contacts, contactNodeCreate( socketDescriptor, buffer ) );

    printf( "Contato adicionado com sucesso.\n" );

    return -1;
}

void deleteContact()
{
    char buffer[81];

    printf( "Digite o nome do contato:\t" );
    scanf( "%80s", buffer );

    ContactNode *deleted = contactListSearch( contacts, buffer );

    if ( deleted == NULL )
        printf( "Não há contato esse nome na sua lista de contatos.\n" );
    else
    {
        pthread_rwlock_wrlock( &socketSetSync );
        FD_CLR( deleted->socket, &socketSet );
        pthread_rwlock_unlock( &socketSetSync );

        contactListRemove( contacts, deleted );
        printf( "Contato removido com sucesso.\n" );
    }
}

void listContact()
{
    pthread_rwlock_wrlock( &socketSetSync );

    ContactNode *current = contacts->first;
    int i = 1;

    while ( current != NULL )
    {
        printf( "Contato %d\n", i++ );
        contactNodePrint( current );
        current = current->next;
    }

    pthread_rwlock_unlock( &socketSetSync );
}

// Funções para threads INICIO
void *selecter( void *p )
{
    struct timeval timeout;
    int result;

    while ( 1 )
    {
        timeout.tv_sec = 2; // Timeout a cada 2 segundos, para que o conjunto de sockets possa ser atualizado.
        timeout.tv_usec = 0;

        pthread_rwlock_rdlock( &socketSetSync );
        result = select( FD_SETSIZE, &socketSet, NULL, NULL, &timeout );
        pthread_rwlock_unlock( &socketSetSync );

        if ( result > 0 ) // Se algum socket recebeu dados.
        {
            /* TODO: Tratar leitura */
        }
        else if ( result == -1 )
        {
            perror( "Erro em select" );
            return errno;
        }
    }
}
// Funções para threads FIM

int main()
{
    // Definir mensagens na linguagem do sistema.
    setlocale( LC_ALL, "" );

    //Declaração das threads
    pthread_t threads[THREAD_NUM];

    // Inicilização do conjunto de sockets e do mutex para sincronizá-lo.
    FD_ZERO( &socketSet );
    if ( pthread_rwlock_init( &socketSetSync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar socketSetSync" );
        return -1;
    }

    // Inicialização da lista de contatos.
    if ( ( contacts = contactListCreate() ) == NULL )
    {
        fprintf( stderr, "Erro ao criar lista de contatos.\n" );
        return -1;
    }

    // Inicialização das threads.
    threadID[THREAD_LISTENER] = pthread_create( &threads[THREAD_LISTENER], NULL, /*LISTEN()*/, NULL);
    threadID[THREAD_ACCEPTER] = pthread_create( &threadID[THREAD_ACCEPTER], NULL, /*ACCEPT()*/, NULL);
    threadID[THREAD_SELECTER] = pthread_create( &threads[THREAD_SELECTER], NULL, selecter, NULL);

    // Destruição das estruturas alocadas.
    if ( contacts != NULL )
        contactListDestroy( contacts );

    if ( pthread_rwlock_destroy( &socketSetSync ) != 0 )
    {
        perror( "Erro ao destruir trava de leitura e escrita" );
        return -1;
    }
}
