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

    const char message[  ] = "Conectado com sucesso.\n";
    int errornum, socketDescriptor;
    unsigned int port;

    // INICIO Leitura e conversão do endereço.
    struct sockaddr_in sa;
    char buffer[ INET_ADDRSTRLEN ]; // Buffer para string de endereço.

    socketDescriptor = socket( PF_INET, SOCK_STREAM, 0 );

    sa.sin_family = AF_INET;
    printf( "Digite um endereço IP:\t" );
    scanf( "%15s", buffer );

    // Checagem de erro
    if ( (errornum = inet_pton( AF_INET, buffer, &sa.sin_addr )) < 1 )
    {
        fprintf( stderr, "Erro ao converter string para endereço: %d\n", errornum );
        return errornum;
    }

    printf( "Digite uma porta:\t" );
    scanf( "%5u", &port );

    // Checagem de erro
    if ( (errornum = (sa.sin_port = htons( port ))) < 1 )
    {
        fprintf( stderr, "Erro ao converter porta para endereço: %d\n", errornum );
        return errornum;
    }
    // FIM Leitura e conversão do endereço.

    if ( connect( socketDescriptor, (struct sockaddr *)&sa, sizeof( sa ) ) == -1 )
    {
        perror( "Erro em connect" );
        return errno;
    }

    char received[100];
    if ( recv( socketDescriptor, received, 100 * sizeof( char ), 0 ) == -1 )
    {
        perror( "Erro em recv" );
        return errno;
    }

    printf( "Message: %s", received );

    close( socketDescriptor );

    return 1;
}
