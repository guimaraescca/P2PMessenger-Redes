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
#define messageSize 2000

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

void pendingAlerts(){}
void exitMenu(){}

//=================== ALERT MENU ===============================================
//Impressão de avisos de confirmação de envio ou erros.
void alertMenu(const char* alert){
    system("clear");
    printf("\n%s \n\n", alert);
}

//=================== ADD CONTACT ==============================================
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

        if ( contactListSearch( contacts, buffer ) != NULL ){
            printf( "Nome já utilizado!\n");
        }else
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

    alertMenu("Contato adicionado com sucesso.");
    
    return -1;
}

//=================== DELETE CONTACT ===========================================
void deleteContact()
{
    char buffer[81];

    printf( "Digite o nome do contato, não utilize espaços e máximo de 80 caracteres:\n" );
    scanf( "%80s", buffer );

    ContactNode *deleted = contactListSearch( contacts, buffer );

    // TODO: contactListSearch e Remove deadlock.

    if ( deleted == NULL ){
        alertMenu("Não há contato com esse nome na sua lista de contatos.");
    }else
    {
        pthread_rwlock_wrlock( &socketSetSync );
        FD_CLR( deleted->socket, &socketSet );
        pthread_rwlock_unlock( &socketSetSync );

        pthread_rwlock_unlock( &contacts->sync );
        contactListRemove( contacts, deleted );
        alertMenu("Contato removido com sucesso.");    
    }
}

//=================== LIST CONTACTS ============================================
void listContact()
{
    pthread_rwlock_rdlock( &contacts->sync );

    ContactNode *current = contacts->first;
    int i = 1;

    while ( current != NULL )
    {
        printf( "Contato %d\n", i++ );
        contactNodePrint( current );
        current = current->next;
    }

    pthread_rwlock_unlock( &contacts->sync );
    printf("Pressione ENTER para voltar ao menu principal.\n");
    getchar();
    alertMenu(" ");
}

//=================== SEND MESSAGE =============================================
void sendMessage(){
    
    int sendResult;
    char buffer[81];
    char* message;  //TODO: Trocar por const char* ?
    ContactNode* receiver;

    printf("Digite o nome do contato:\t");
    scanf("%80s", buffer);

    receiver = contactListSearch(contacts, buffer);

    if ( receiver == NULL ){
        pthread_rwlock_rdlock( &contacts->sync );
        alertMenu( "Não há contato com esse nome na sua lista de contatos!" );
    }else{
        printf("Mensagem: ");
        fgets(message, messageSize, stdin);
        
        sendResult = send( receiver->socket, message, (strlen(message) + 1) * sizeof(char), 0 );
        
        pthread_rwlock_unlock( &contacts->sync );

        if(sendResult == -1){
            //TODO: Tratar 'errno' talvez
            alertMenu("Falha no envio!");
        }else{
            alertMenu("Mensagem enviada!");
        }
    }
}

//=================== BROADCAST MESSAGE ========================================
void broadcastMessage(){
    
    int receiverId;
    char message[messageSize];
    char buffer[4*contacts->size];
    char* token;
    ContactNode *receiver;
    
    printf("Envio de mensagem brodcast.\n");
    printf("Mensagem broadcast: ");
    fgets(message, messageSize, stdin);
    
    printf("Insira os IDs dos contatos para quem deseja enviar a mensagem seguidos de <ENTER>.\n\n");
    
    //Exibiçao dos IDs da lista
    pthread_rwlock_rdlock( &contacts->sync );

    ContactNode *current = contacts->first;
    while ( current != NULL )
    {
        contactNodePrint( current );
        current = current->next;
    }

    pthread_rwlock_unlock( &contacts->sync );

    
    printf("\nEx: 105 201 110 <ENTER>\n");
    fgets(buffer, (4*contacts->size), stdin );

    //Aquisição do primeiro ID dentro do buffer
    token = strtok (buffer," ");
    receiverId = atoi(token);

    while (token != NULL){
        //Pesquisa do ID dentro da lista e envio da mensagem.
        receiver = contactListSearchId(receiverId);
        send( receiver->socket, message, (strlen(message) + 1) * sizeof(char), 0 );
        pthread_rwlock_unlock( &contacts->sync ); //Para cada pesquisa feita é precisa liberar a trava adquirida.
        
        //Aquisisão do próximo ID no buffer. 
        token = strtok (NULL, " ");
        receiverId = atoi(token);
    }
    alertMenu("Mensagem broadcast enviada!");
}

//=================== MAIN MENU ================================================
void menu(){
    
    int input;

    do{
        printf("Menu\n\n");
    
        printf("1 - Adicionar contato.\n");
        printf("2 - Listar contatos.\n");
        printf("3 - Deletar contato.\n");
        printf("4 - Enviar mensagem.\n");
        printf("5 - Enviar mensagem em grupo.\n");
        printf("6 - Alertas pendentes.\n");
        printf("0 - Fechar programa.\n\n");
    
        printf("~$ ");
        scanf("%d", &input);
    
        switch(input) {
          	case 1:
        	    addContact();
      		    break;
      	    case 2:
        		listContact();
        		break;
          	case 3:
        		deleteContact();
        		break;
            case 4:
                sendMessage();
      		    break;
            case 5:
                broadcastMessage();
      		    break;
            case 6:
      		    break;
            case 0:
                break;
       	    default:
                alertMenu("Opção inválida! Tente novamente.");
        	    break;  	 	
      	}
    }while(input != 0);
    
    return;
}

//==============================================================================
//=================== THREADS FUNCTIONS ========================================
//==============================================================================

//=================== READING THREAD ===========================================
void *reader( void *p )
{
    const ContactNode *node = (ContactNode *)p;

    size_t size, bufferSize;
    ssize_t partial, total = 0;

    while ( total < sizeof( ssize_t ) )
    {
        partial = recv( node->socket, (void *)(&size + total), sizeof( ssize_t ) - total, 0 );
        if ( partial == -1 )
        {
            perror( "Erro na leitura do socket" );
            return errno;
        }
        total += partial;
    }
    char buffer[size];
    
    bufferSize = size * sizeof( char );
    total = 0;
    while ( total < size )
    {
        partial = recv( node->socket, (void *)(buffer + total), bufferSize - total, 0 );
        if ( partial == -1 )
        {
            perror( "Erro na leitura do socket" );
            return errno;
        }
        total += partial;
    }
    // TODO: Definir como e quando o usuário lerá as mensagens - Arquivos?
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
                if ( FD_ISSET( curent->socket, &socketSet != 0 ) )
                {
                    // TODO: ID[i] = pthread_create( &threads[i], NULL, receiver, (void *)current);
                    --i;
                }
                current = current->next;
            }

            pthread_rwlock_unlock( &contacts->sync );
            pthread_rwlock_unlock( &socketSetSync );

            for ( i = 0; i < result; ++i )
                thread_join( ID[i] );
        }
        else if ( result == -1 )
        {
            perror( "Erro em select" );
            pthread_rwlock_unlock( &contacts->sync );
            pthread_rwlock_unlock( &socketSetSync );
            return errno;
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

int createServer(  )
{   //TODO: ajustar bind
    int socketDescriptor;

    // Abertura do socket.
    socketDescriptor = socket( PF_INET, SOCK_STREAM, 0 );
    if ( socketDescriptor == -1 )
    {
        perror( "Erro ao abrir socket" );
        return errno;
    }

    // Associação do socket a uma porta.
    struct sockaddr_in this_machine;

    this_machine.sin_family = AF_INET;  // IPv4.
    this_machine.sin_port = htons( port );
    this_machine.sin_addr.s_addr = htonl( INADDR_ANY ); // IP da máquina.

    if ( bind( socketDescriptor, (struct sockaddr *)&this_machine, sizeof( this_machine ) ) == -1 )
    {
        perror( "Erro ao realizar bind no socket" );
        return errno;
    }

    // Criar fila de solicitações de conexão.
    if ( listen( socketDescriptor, 50 ) == -1 )
    {
        perror( "Erro em listen" );
        return errno;
    }
    
    // TODO: Chamar thread accepter.
}

//==============================================================================
//=================== MAIN =====================================================
//==============================================================================

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
    threadID[THREAD_ACCEPTER] = pthread_create( &threads[THREAD_ACCEPTER], NULL, /*ACCEPT()*/, NULL);
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
