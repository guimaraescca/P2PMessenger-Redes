#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h> // Contém função close.

#include <netdb.h>
#include <arpa/inet.h> // Conversões para formato de rede.

int main( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        fprintf( stderr, "Erro ! Forneça uma porta como argumento.\n" );
        return -1;
    }

    const char message[  ] = "Conectado com sucesso.\n";
    int errornum, socketDescriptor;
    unsigned int port;
    sscanf( argv[1], "%5u", &port );

    // INICIO Abertura do socket.
    socketDescriptor = socket( PF_INET, SOCK_STREAM, 0 );
    // Checagem de erro
    if ( socketDescriptor == -1 )
    {
        perror( "Erro ao abrir socket" );
        return errno;
    }
    // FIM Abertura do socket.

    // INICIO Associação do socket a uma porta.
    struct sockaddr_in this_machine;

    this_machine.sin_family = AF_INET;  // IPv4.
    this_machine.sin_port = htons( port );
    this_machine.sin_addr.s_addr = htonl( INADDR_ANY ); // IP da máquina.

    if ( bind( socketDescriptor, (struct sockaddr *)&this_machine, sizeof( this_machine ) ) == -1 )
    {
        perror( "Erro ao realizar bind no socket" );
        return errno;
    }
    // FIM Associação do socket a uma porta.

    // INICIO Enfileirar solicitações de conexão.
    if ( listen( socketDescriptor, 10 ) == -1 )
    {
        perror( "Erro em listen" );
        return errno;
    }
    // FIM Enfileirar solicitações de conexão.

    // INICIO Aceitar conexão.
    struct sockaddr client;

    socklen_t wtv = sizeof(struct sockaddr_storage);
    int send_connection;
    if ( (send_connection = accept( socketDescriptor, &client, &wtv )) == -1 )
    {
        perror( "Erro em accept" );
        return errno;
    }
    // FIM Aceitar conexão.

    // INICIO Envio de mensagem.
    if ( send( send_connection, message, (strlen(message)  + 1) * sizeof(char), 0 ) == -1 )
    {
        perror( "Erro em send" );
        return errno;
    }
    // FIM Envio de mensagem.

    close( socketDescriptor );

    return 1;
}
