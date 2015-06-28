#include <interface.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <global.h>

#include <netdb.h>
#include <arpa/inet.h> // Conversões para formato de rede.

#include <sys/types.h>

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

    // Criação do nome do contato.
    int notUsed = 0;
    pthread_rwlock_rdlock( &contacts->sync );
    do
    {
        printf( "Digite um nome para o contato:\t" );
        scanf( "%80s", buffer );

        if ( contactListSearch( contacts, buffer ) != NULL ){
            printf( "Nome já utilizado!\n");
        }else
            notUsed = 1;
    } while ( notUsed == 0 ); // Enquanto o nome fornecido já tiver sido utilizado.
    pthread_rwlock_unlock( &contacts->sync );

    // Estabelecimento de conexão.
    if ( connect( socketDescriptor, (struct sockaddr *)&sa, sizeof( sa ) ) == -1 )
    {
        alertMenu( "Falha ao adicionar contato!" );
        perror( "Não foi possível estabelecer conexão" );
        return -1;
    }
    
    ssize_t total = 0, partial, nameSize = strlen( serverName ), 
        nameCharSize = sizeof( char ) * nameSize;
    
    // Envia o tamanho do nome do servidor local.
    while ( total < sizeof( total ) )
    {
        partial = send( socketDescriptor, (void *)&nameSize + total, sizeof( nameSize ) - total, 0 );
        if ( partial == -1 )
        {
            alertMenu( "Falha ao adicionar contato!" );
            perror( "Não foi possível transmitir o tamanho do nome local" );
            close( socketDescriptor );
            return -1;
        }
        total += partial;
    }
    total = 0;
    // Envia o nome local.
    while ( total < nameCharSize )
    {
        partial = send( socketDescriptor, (void *)&nameSize + total, nameCharSize - total, 0 );
        if ( partial == -1 )
        {
            alertMenu( "Falha ao adicionar contato!" );
            perror( "Não foi possível transmitir o nome local" );
            close( socketDescriptor );
            return -1;
        }
        total += partial;
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

    pthread_rwlock_wrlock( &contacts->sync );
    ContactNode *deleted = contactListSearch( contacts, buffer );

    if ( deleted == NULL ){
        alertMenu("Não há contato com esse nome na sua lista de contatos.");
    }else
    {
        pthread_rwlock_wrlock( &socketSetSync );
        FD_CLR( deleted->socket, &socketSet );
        pthread_rwlock_unlock( &socketSetSync );

        contactListRemove( contacts, deleted );

        alertMenu("Contato removido com sucesso.");    
    }
    
    pthread_rwlock_unlock( &contacts->sync );
}

//=================== LIST CONTACTS ============================================
void listContact()
{
    pthread_rwlock_rdlock( &contacts->sync );

    ContactNode *current = contacts->first;

    while ( current != NULL )
    {
        contactNodePrint( current );
        current = current->next;
    }

    pthread_rwlock_unlock( &contacts->sync );
    printf("\n\tPressione <ENTER> para voltar ao menu principal.\n");
    getchar();  //TODO: Essa pausa nao ta funcionando como deveria. :(
    alertMenu(" ");
}

void messageMenu(){
    
    int contactId, msgNumber;
    ContactNode* sender;        //De quem se deseja ler as mensagens
    GenericNode* messageNode;   //Mensagem que será lida.
    char* message;
    
    printf("Digite o ID do contato para ver as mensagens recebidas:\t");
    scanf("%d", &contactId);
    
    pthread_rwlock_wrlock( &contacts->sync );
    sender = contactListSearchId(contacts, contactId);
    
    if( sender == NULL ){
        pthread_rwlock_unlock( &contacts->sync );
        alertMenu("ID de contato não encontrado!");
        return;
    }else{
        if(sender->messages->head == NULL){    //Não há mensagens a serem lidas
            pthread_rwlock_unlock( &contacts->sync );
            alertMenu("Não há mensagens a serem lidas para este contato!");
            return;
        }
        
        while(sender->messages->head != NULL){ //Verifica se existem msgs a serem lidas
            
            messageNode = dequePopFront(sender->messages);
            
            printf("> %s\n", (char *)messageNode->item);
        }
        pthread_rwlock_unlock( &contacts->sync );
        printf("\n\tPressione <ENTER> para voltar ao menu principal.\n");
        getchar();
        alertMenu(" ");
        return;
    }
}

//=================== SEND MESSAGE =============================================
void sendMessage(){
    
    int sendResult;
    char buffer[81];
    char* message;
    ContactNode* receiver;

    printf("Digite o nome do contato:\t");
    scanf("%80s", buffer);

    pthread_rwlock_wrlock( &contacts->sync );
    receiver = contactListSearch(contacts, buffer);

    if ( receiver == NULL ){
        pthread_rwlock_unlock( &contacts->sync );
        alertMenu( "Não há contato com esse nome na sua lista de contatos!" );
        return;
    }else{
        printf("Mensagem: ");
        fgets(message, messageSize, stdin);
        
        sendResult = send( receiver->socket, message, (strlen(message) + 1) * sizeof(char), 0 );

        if(sendResult == -1){
            alertMenu("Falha no envio!");
            switch ( errno ) {
                case EBADF:
                case ENOTSOCK:
                case ENOTCONN:
                case EPIPE:
                    contactListRemove( contacts, receiver );
                    printf( "O contato não está conectado e foi removido da lista.\n" );
                    break;
                default:
                    perror( "Erro inesperado" );
                    break;
            }
            pthread_rwlock_unlock( &contacts->sync );
            return;
        }else{
            pthread_rwlock_unlock( &contacts->sync );
            alertMenu("Mensagem enviada!");
            return;
            
        }
    }
}

//=================== BROADCAST MESSAGE ========================================
void broadcastMessage(){
    
    int receiverId;
    char message[messageSize];
    char buffer[4*contacts->size];
    char* token;    //Necessário para separar o diferente IDs coletados
    ContactNode *receiver;
    
    printf("Mensagem broadcast: ");
    fgets(message, messageSize, stdin);
    
    printf("Insira os IDs dos contatos para quem deseja enviar a mensagem seguidos de <ENTER>.\n\n");
    
    //Exibiçao dos IDs da lista
    /*TODO: Não será mais necessário devido a decisões de projeto
    pthread_rwlock_rdlock( &contacts->sync );
    .
    ContactNode *current = contacts->first;
    while ( current != NULL )
    {
        contactNodePrint( current );
        current = current->next;
    }

    pthread_rwlock_unlock( &contacts->sync );
    */
    printf("\nEx: 105 201 110 <ENTER>\n");
    fgets(buffer, (4*contacts->size), stdin );

    //Aquisição do primeiro ID dentro do buffer
    token = strtok (buffer," ");
    receiverId = atoi(token);

    while (token != NULL){
        //Pesquisa do ID dentro da lista e envio da mensagem.
        pthread_rwlock_wrlock( &contacts->sync );
        
        receiver = contactListSearchId(contacts, receiverId);
        send( receiver->socket, message, (strlen(message) + 1) * sizeof(char), 0 );
        
        pthread_rwlock_unlock( &contacts->sync );
        
        //Aquisisão do próximo ID no buffer. 
        token = strtok (NULL, " ");
        receiverId = atoi(token);
    }
    alertMenu("Mensagem broadcast enviada!");
    return;
}

void acceptContact() {
    pthread_rwlock_wrlock( &pendingAccept->sync );

    if ( pendingRead->size > 0 ) {
        ContactNode *current = contactListPopFront( pendingRead );
        char accept;

        printf( "Há %d contatos a serem aceitos.\n", pendingRead->size );
        while ( current != NULL ) {
            printf( "Nome: %s\nAceitar [s/n]?", current->name );
            scanf( "%c", &accept );
            if ( accept == 's' ) {
                pthread_rwlock_wrlock( &contacts->sync );
                if ( contactListSearch( contacts, current->name ) != NULL ) {
                    current->name = (char *) realloc( current->name, sizeof( char ) * 81 );
                    do {
                        printf( "O nome já está na sua lista, digite outro nome com até 80 caracteres.\n" );
                        scanf( "%80s", current->name );
                    } while ( contactListSearch( contacts, current->name ) != NULL );
                }
                contactListInsert( contacts, current );
                pthread_rwlock_unlock( &contacts->sync );
                printf( "Contato aceito com sucesso!\n" );
            }
            current = contactListPopFront( pendingRead );
        }
    }
    pthread_rwlock_unlock( &pendingAccept->sync );
    alertMenu( "Não há mais contatos a serem aceitos." );
}

//=================== MAIN MENU ================================================
void menu(){
    
    int input, alerts, read;

    do{
        alerts = read = 0;
        printf("> Menu\n\n");
    
        printf("1 - Adicionar contato.\n");
        printf("2 - Listar contatos.\n");
        printf("3 - Deletar contato.\n");
        printf("4 - Enviar mensagem.\n");
        printf("5 - Enviar mensagem em grupo.\n");
        pthread_rwlock_rdlock( &pendingAccept );
        if ( pendingAccept->size > 0 ){
            alerts = 1;
            printf("6 - Adições pendentes.\n");
        }
        pthread_rwlock_unlock( &pendingAccept );
        pthread_rwlock_rdlock( &pendingRead );
        if ( pendingRead->size > 0 ) {
            read = 1;
            printf("7 - Ler mensagens.\n");
        }
        pthread_rwlock_unlock( &pendingRead );
        printf("0 - Fechar programa.\n\n");
    
        printf("~$ ");
        scanf("%d", &input);
    
        switch(input) {
          	case 1:
          	    alertMenu("> Adicionar contato");
        	    addContact();
      		    break;
      	    case 2:
      	        alertMenu("> Listar contatos");
        		listContact();
        		break;
          	case 3:
          	    alertMenu("> Deletar contato");
        		deleteContact();
        		break;
            case 4:
                alertMenu("> Envio de mensagem");
                sendMessage();
      		    break;
            case 5:
                alertMenu("> Envio de mensagem em grupo");
                broadcastMessage();
      		    break;
            case 6:
                if ( alerts == 0 )
                    alertMenu("Opção inválida! Tente novamente.");
                else {
                    alertMenu("> Aceitar contatos");
                    acceptContact();
                }
      		    break;
      		case 7:
      		    if ( read == 0 )
      		        alertMenu("Opção inválida! Tente novamente.");
      		    else {
                    alertMenu("> Leitura de mensagens");
      		        messageMenu();
      		    }
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