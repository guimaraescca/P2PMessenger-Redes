#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h> // Para usar select e fd_set.

#include <unistd.h> // Contém função close.

#include <netdb.h>
#include <arpa/inet.h> // Conversões para formato de rede.

#include <generic_deque.h>
#include <contact_list.h>
#include <global.h>

#include <interface.h>

//==============================================================================
//=================== THREADS FUNCTIONS ========================================
//==============================================================================

//=================== READING THREAD ===========================================
void *reader( void *p )
{
    ContactNode *node = (ContactNode *)p;
    ssize_t size, bufferSize, partial, total = 0;

    while ( total < sizeof( size ) )
    {
        partial = recv( node->socket, (void *)(&size + total), sizeof( size ) - total, 0 );
        if ( partial == -1 )
        {
            perror( "Erro na leitura do socket" );
            return (void *)-1;
        }
        total += partial;
    }
    char buffer[size];
    bufferSize = sizeof( char ) * size;
    total = 0;
    while ( total < bufferSize )
    {
        partial = recv( node->socket, (void *)(buffer + total), bufferSize - total, 0 );
        if ( partial == -1 )
        {
            perror( "Erro na leitura do socket" );
            return (void *)-1;
        }
        total += partial;
    }
    dequePushBack( node->messages, nodeCreate( buffer, bufferSize ) );

    pthread_exit(0);
}

//=================== SELECTER THREAD ==========================================
void *selecter( void *p )
{
    struct timeval timeout;
    int result;

    do
    {
        timeout.tv_sec = 2; // Timeout a cada 2 segundos, para que o conjunto de sockets possa ser atualizado.
        timeout.tv_usec = 0;

        pthread_rwlock_rdlock( &socketSetSync );
        result = select( FD_SETSIZE, &socketSet, NULL, NULL, &timeout );

        pthread_rwlock_rdlock( &contacts->sync );
        
        if ( result > 0 ) // Se algum socket recebeu dados.
        {
            ContactNode *current = contacts->first;
            pthread_t threads[result];
            int i = result - 1, ID[result];

            while ( current != NULL )
            {
                if ( FD_ISSET( current->socket, &socketSet ) != 0 )
                {
                    ID[i] = pthread_create( &threads[i], NULL, reader, (void *)current);
                    --i;
                }
                current = current->next;
            }

            pthread_rwlock_unlock( &contacts->sync );
            pthread_rwlock_unlock( &socketSetSync );    

            for ( i = 0; i < result; ++i )
                pthread_join( ID[i], NULL );

            pthread_rwlock_wrlock( &pendingReadSync );
            pendingRead = 1;
            pthread_rwlock_unlock( &pendingReadSync );
        }
        else if ( result == -1 )
        {
            perror( "Erro em select" );
            pthread_rwlock_unlock( &contacts->sync );
            pthread_rwlock_unlock( &socketSetSync );
            return (void *)-1;
        }
        else
        {
            pthread_rwlock_unlock( &contacts->sync );
            pthread_rwlock_unlock( &socketSetSync );
        }
    } while(1);
}

//==============================================================================
//=================== SERVER MANIPULATION ======================================
//==============================================================================

void *accepter( void *p )
{
    int clientSocket;
    struct sockaddr_in addr;
    socklen_t length;
    char name[81];

    while ( 1 )
    {
        length = sizeof(struct sockaddr_in);
        
        // Aceita a conexão pendente.
        if ( (clientSocket = accept( serverSocket, (struct sockaddr*)&addr, &length )) == -1 )
        {
            perror( "Erro ao obter socket para o cliente" );
            return (void *)-1;
        }
        ssize_t total = 0, partial, nameSize;
        // Recebe o tamanho do nome do servidor local.
        while ( total < sizeof( nameSize ) )
        {
            partial = recv( clientSocket, (void *)&nameSize + total, sizeof( nameSize ) - total, 0 );
            if ( partial == -1 )
            {
                perror( "Erro ao transmitir tamanho do nome local" );
                return (void *)-1;
            }
            total += partial;
        }
        total = 0;
        // Recebe o nome local.
        nameSize *= sizeof( char );
        while ( total < nameSize )
        {
            partial = recv( clientSocket, (void *)(name + total), nameSize - total, 0 );
            if ( partial == -1 )
            {
                perror( "Erro ao transmitir tamanho do nome local" );
                return (void *)-1;
            }
            total += partial;
        }
        contactListInsert( pendingAccept, contactNodeCreate( clientSocket, name ) );
    }

}

int createServer()
{
    serverName = (char *) malloc( sizeof(char) * 81 );
    // Ler nome do usuário local.
    printf( "Digite um nome de usuário, de até 80 caracteres:\n" );
    scanf( "%80s", serverName );
    
    // Abertura do socket.
    serverSocket = socket( PF_INET, SOCK_STREAM, 0 );
    if ( serverSocket == -1 )
    {
        perror( "Erro ao abrir socket" );
        return -1;
    }

    // Associação do socket a uma porta.
    struct sockaddr_in this_machine;

    this_machine.sin_family = AF_INET;  // IPv4.
    this_machine.sin_port = htons( SERVER_PORT );
    this_machine.sin_addr.s_addr = htonl( INADDR_ANY ); // IP da máquina.

    if ( bind( serverSocket, (struct sockaddr *)&this_machine, sizeof( this_machine ) ) == -1 )
    {
        perror( "Erro ao realizar bind no socket" );
        return -1;
    }

    // Criar fila de solicitações de conexão.
    if ( listen( serverSocket, 50 ) == -1 )
    {
        perror( "Erro em listen" );
        return -1;
    }
    return 0;
}

//==============================================================================
//=================== MAIN =====================================================
//==============================================================================

int main()
{
    // Definir mensagens na linguagem do sistema.
    setlocale( LC_ALL, "" );
    
    pthread_t threads[THREAD_NUM];

    // Inicialização do conjunto de sockets e do mutex para sincronizá-lo.
    FD_ZERO( &socketSet );
    if ( pthread_rwlock_init( &socketSetSync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar socketSetSync" );
        return -1;
    }
    if ( pthread_rwlock_init( &pendingReadSync, NULL ) != 0 )
    {
        perror( "Erro ao inicializar pendingReadSync" );
        return -1;
    }

    // Inicialização das listas.
    contacts = contactListCreate();
    if ( contacts == NULL )
    {
        fprintf( stderr, "Erro ao criar lista de contato.\n" );
        return -1;
    }
    pendingAccept = contactListCreate();
    if ( pendingAccept == NULL )
    {
        fprintf( stderr, "Erro ao criar lista de contatos a adicionar.\n" );
        return -1;
    }

    alertMenu(" ");
    // Criação do servidor.
    if ( createServer() == -1 )
        return -1;

    // Inicialização das threads.
    threadID[THREAD_ACCEPTER] = pthread_create( &threads[THREAD_ACCEPTER], NULL, accepter, NULL);
    threadID[THREAD_SELECTER] = pthread_create( &threads[THREAD_SELECTER], NULL, selecter, NULL);

    // Chamada da interface principal.
    alertMenu(" ");
    menu();
    
    // Destruição das estruturas alocadas.
    if ( contacts != NULL )
        contactListDestroy( contacts );

    if ( pendingAccept != NULL )
        contactListDestroy( pendingAccept );

    if ( pthread_rwlock_destroy( &socketSetSync ) != 0 )
    {
        perror( "Erro ao destruir trava socketSetSync" );
        return -1;
    }
    if ( pthread_rwlock_destroy( &pendingReadSync ) != 0 )
    {
        perror( "Erro ao destruir trava pendingReadSync" );
        return -1;
    }
    close( serverSocket );
    return 0;
}
