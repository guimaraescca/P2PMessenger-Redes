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

#include <string>
#include <iostream>

#include <interface.h>

using namespace std;

//==============================================================================
//=================== THREADS FUNCTIONS ========================================
//==============================================================================

//=================== READING THREAD ===========================================
void *reader( void *p )
{
    ContactNode *node = (ContactNode *)p;
    ssize_t size, bufferSize, partial;

    if ( recv( node->socket, (void *)&size, sizeof( size ), 0 ) == -1 )
    {
        perror( "Erro na leitura do socket" );
        return (void *)-1;
    }
    char buffer[size/sizeof(char)];
    if ( recv( node->socket, (void *)buffer, size, 0 ) == -1 )
    {
        perror( "Erro na leitura do socket" );
        return (void *)-1;
    }
    cout << "Mensagem recebida: " << buffer << endl;
    dequePushBack( node->messages, nodeCreate( buffer, size ) );

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

        result = select( FD_SETSIZE, &socketSet, NULL, NULL, &timeout );

        if ( result > 0 ) // Se algum socket recebeu dados.
        {
            cout << "SELECTER: leitura detectada " << endl;
            ContactNode *current = contacts->first;
            pthread_t threads[result];
            int i = result - 1, ID[result];
            void *threadReturn[result];

            while ( current != NULL )
            {
                if ( FD_ISSET( current->socket, &socketSet ) != 0 )
                {
                    ID[i] = pthread_create( &threads[i], NULL, reader, (void *)current);
                    --i;
                }
                current = current->next;
            }

            pendingRead = 1;

            for ( i = 0; i < result; ++i )
                pthread_join( ID[i], &threadReturn[i] );

        }
        else {
            if ( result == -1 ){
                perror( "Erro em select" );
                return (void *)-1;
            }
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
    char *name;

    while ( 1 )
    {
        length = sizeof(struct sockaddr_in);

        // Aceita a conexão pendente.
        if ( (clientSocket = accept( serverSocket, (struct sockaddr*)&addr, &length )) == -1 )
        {
            perror( "Erro ao obter socket para o cliente" );
            return (void *)-1;
        }
        ssize_t nameSize;

        recv( clientSocket, (void *)&nameSize, sizeof( nameSize ), 0 );
        name = (char *) malloc( nameSize );
        recv( clientSocket, (void *)name, nameSize, 0 );

        contactListInsert( pendingAccept, contactNodeCreate( clientSocket, string(name) ) );

        free(name);
    }

}

int createServer()
{
    // Ler nome do usuário local.
    cout <<  "Digite um nome de usuário, de até 80 caracteres:" << endl ;
    getline( cin, serverName );

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

    close( serverSocket );
    return 0;
}
