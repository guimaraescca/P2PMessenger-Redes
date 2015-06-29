#include <interface.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <global.h>

#include <netdb.h>
#include <arpa/inet.h> // Conversões para formato de rede.

#include <sys/types.h>

#include <unistd.h>

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
    char* buffer;
    struct sockaddr_in sa;

    sa.sin_family = AF_INET;
    buffer = (char*)malloc( 81*sizeof(char) );
    
    printf( "Digite o endereço IP: " );
    scanf( "%15s", buffer );

    // Conversão da string em endereço IP.
    if ( (errornum = inet_pton( sa.sin_family, buffer, &sa.sin_addr )) < 1 )
    {
        fprintf( stderr, "Erro ao converter string para endereço: %d\nVerifique se digitou um IP no formato correto.\n", errornum );
        return errornum;
    }

    // Seleção da porta.
    if ( (errornum = (sa.sin_port = htons( SERVER_PORT ))) < 1 )
    {
        fprintf( stderr, "Erro ao converter inteiro para formato de rede: %d\nVerifique se digitou uma porta válida.\n", errornum );
        return errornum;
    }

    // Criação do nome do contato.
    int notUsed = 0;
    pthread_rwlock_rdlock( &contacts->sync );
    do
    {
        printf( "Digite um nome para o contato:\t" );
        scanf( "%s", buffer );

        if ( contactListSearch( contacts, buffer ) != NULL ){
            printf( "Nome já utilizado!\n");
        }else
            notUsed = 1;
    } while ( notUsed == 0 ); // Enquanto o nome fornecido já tiver sido utilizado.
    pthread_rwlock_unlock( &contacts->sync );
    
    printf( "Nome válido: %s", buffer );
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
        partial = send( socketDescriptor, (void *)(&nameSize + total), sizeof( nameSize ) - total, 0 );
        if ( partial == -1 )
        {
            alertMenu( "Falha ao adicionar contato!" );
            perror( "Não foi possível transmitir o tamanho do nome local" );
            close( socketDescriptor );
            return -1;
        }
        total += partial;
    }
    printf( "Tamanho do nome enviado.\n" );
    total = 0;
    // Envia o nome local.
    while ( total < nameCharSize )
    {
        partial = send( socketDescriptor, (void *)(&nameSize + total), nameCharSize - total, 0 );
        if ( partial == -1 )
        {
            alertMenu( "Falha ao adicionar contato!" );
            perror( "Não foi possível transmitir o nome local" );
            close( socketDescriptor );
            return -1;
        }
        total += partial;
    }
    printf( "Nome enviado.\n" );
    // Atualizar conjunto de sockets.
    pthread_rwlock_wrlock( &socketSetSync );
    FD_SET( socketDescriptor, &socketSet );
    pthread_rwlock_unlock( &socketSetSync );

    contactListInsert( contacts, contactNodeCreate( socketDescriptor, buffer ) );

    alertMenu("Contato adicionado com sucesso.");
    
    return 1;
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
    
    if ( current == NULL )
        alertMenu( "Lista de contatos vazia!" );
    else {
        alertMenu("");
        do {
            contactNodePrint( current );
            current = current->next;
        } while( current != NULL );
    
        pthread_rwlock_unlock( &contacts->sync );
    }
    
    pthread_rwlock_wrlock( &pendingReadSync );
    pendingRead = 0;
    pthread_rwlock_unlock( &pendingReadSync );
}

void messageMenu(){
    
    int contactId;
    ContactNode* sender;        //De quem se deseja ler as mensagens
    GenericNode* messageNode;   //Mensagem que será lida.
    
    printf("Digite o ID do contato para ver as mensagens recebidas:\t");
    scanf("%d", &contactId);
    
    pthread_rwlock_wrlock( &contacts->sync );
    sender = contactListSearchId(contacts, contactId);
    
    if( sender == NULL ) {
        pthread_rwlock_unlock( &contacts->sync );
        alertMenu("ID de contato não encontrado!");
    }
    else {
        if(sender->messages->head == NULL){    //Não há mensagens a serem lidas
            pthread_rwlock_unlock( &contacts->sync );
            alertMenu("Não há mensagens a serem lidas para este contato!");
        }
        else {
            while(sender->messages->head != NULL){ //Verifica se existem msgs a serem lidas
                messageNode = dequePopFront(sender->messages);
                printf("> %s\n", (char *)messageNode->item);
            }
            pthread_rwlock_unlock( &contacts->sync );
        }
    }
}

//=================== SEND MESSAGE =============================================
void sendMessage(){
    
    int sendResult;
    size_t bufferSize = 81;
    char* buffer;
    char* message;
    ContactNode* receiver;
    
    message = (char*)malloc(bufferSize*sizeof(char));
    buffer = (char*)malloc(bufferSize*sizeof(char));
    
    printf("Digite o nome do contato:\t");
    scanf("%80s", buffer);

    pthread_rwlock_wrlock( &contacts->sync );
    receiver = contactListSearch(contacts, buffer);

    if ( receiver == NULL ){
        pthread_rwlock_unlock( &contacts->sync );
        alertMenu( "Não há contato com esse nome na sua lista de contatos!" );
    }else{
        printf("Mensagem: ");
        getline(&buffer, &bufferSize, stdin);
        strcpy(message, buffer);
        
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
                    printf( "Tente enviar novamente.\n" );
                    break;
            }
            pthread_rwlock_unlock( &contacts->sync );
        }else{
            pthread_rwlock_unlock( &contacts->sync );
            alertMenu("Mensagem enviada!");
        }
    }
}

//=================== BROADCAST MESSAGE ========================================
void broadcastMessage(){
    
    int receiverId;
    size_t bufferSize;
    char* buffer;
    char* message;
    char* token;    //Necessário para separar os diferente IDs coletados
    ContactNode *receiver;
    
    bufferSize = messageSize;
    message = (char*)malloc(bufferSize*sizeof(char));

    printf("Mensagem broadcast: ");
    getline(&message, &bufferSize, stdin);

    printf("\nInsira os IDs dos contatos para quem deseja enviar a mensagem seguidos de <ENTER>.\n");
    printf("Ex: 105 201 110 <ENTER>\n");
    
    bufferSize = 5*contacts->size;
    buffer = (char*)malloc(bufferSize*sizeof(char));
    getline(&buffer, &bufferSize, stdin);

    //Aquisição do primeiro ID dentro do buffer
    token = strtok (buffer," ");
    receiverId = atoi(token);

    while (token != NULL){
        //Pesquisa do ID dentro da lista e envio da mensagem.
        pthread_rwlock_wrlock( &contacts->sync );
        
        receiver = contactListSearchId(contacts, receiverId);
        if(receiver == NULL){
            printf("Erro ao endereçar o nó inválido de contato!\n\n");
            token = NULL;
            
            free(buffer);
            free(message);
            return;
        }else{
            send( receiver->socket, message, (strlen(message) + 1) * sizeof(char), 0 );
            
            pthread_rwlock_unlock( &contacts->sync );
            
            //Aquisisão do próximo ID no buffer. 
            token = strtok (NULL, " ");
            receiverId = atoi(token);
        }
    }
    
    free(buffer);
    free(message);
    alertMenu("Mensagem broadcast enviada!");
}

void acceptContact() {
    pthread_rwlock_wrlock( &pendingAccept->sync );

    if ( pendingAccept->size > 0 ) {
        ContactNode *current = contactListPopFront( pendingAccept, 0 );
        char accept[2];

        printf( "Há %d contatos a serem aceitos.\n", pendingAccept->size );
        while ( current != NULL ) {
            printf( "Nome: %s\nAceitar [s/n]?", current->name );
            scanf( "%s", accept );
            if ( accept[0] == 's' ) {
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
    
                pthread_rwlock_wrlock( &socketSetSync );
                FD_SET( current->socket, &socketSet );
                pthread_rwlock_unlock( &socketSetSync );
                printf( "Contato aceito com sucesso!\n" );
            }
            current = contactListPopFront( pendingAccept, 0 );
        }
    }
    pthread_rwlock_unlock( &pendingAccept->sync );
    alertMenu( "Não há mais contatos a serem aceitos." );
}

//=================== MAIN MENU ================================================
void menu(){
    
    int input, alerts;

    do{
        alerts = 0;
        printf("=============================================\n\t[ Menu ] - Bem vindo(a) %s\n=============================================\n\n", serverName);
        
        pthread_rwlock_rdlock( &pendingReadSync );
        if ( pendingRead == 1 )
            printf( "Aviso! Você recebeu novas mensagens!\n\n" );
        pthread_rwlock_unlock( &pendingReadSync );
    
        printf("[1] - Adicionar contato.\n");
        printf("[2] - Listar contatos.\n");
        printf("[3] - Deletar contato.\n");
        printf("[4] - Enviar mensagem.\n");
        printf("[5] - Enviar mensagem em grupo.\n");

        pthread_rwlock_rdlock( &pendingReadSync );
        if ( pendingRead == 1 )
            printf("[6] - Ler mensagens.\n");
        pthread_rwlock_unlock( &pendingReadSync );

        pthread_rwlock_rdlock( &pendingAccept->sync );
        if ( pendingAccept->size > 0 ){
            alerts = 1;
            printf("[7] - Adições pendentes.\n");
        }
        pthread_rwlock_unlock( &pendingAccept->sync );

        printf("[0] - Fechar programa.\n\n=============================================\n");
    
        printf("~$ ");
        
        scanf("%d", &input);
        getchar();  //TODO: Retirar isto aqui.
        
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
                alertMenu("> Leitura de mensagens");
      		    messageMenu();
      		    break;
            case 7:
                if ( alerts == 0 )
                    alertMenu("Opção inválida! Tente novamente.");
                else {
                    alertMenu("> Aceitar contatos");
                    acceptContact();
                }
      		    break;
            case 0:
                break;
      	    default:
                alertMenu("Opção inválida! Tente novamente.");
        	    break;  	 	
      	}
    }while(input != 0);
}