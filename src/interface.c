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

#include <string>
#include <sstream>
#include <iostream>

using namespace std;

//=================== ALERT MENU ===============================================
//Impressão de avisos de confirmação de envio ou erros.
void alertMenu(const char* alert){
    system("clear");
    cout << "\n" << alert << "\n\n";
}

//=================== ADD CONTACT ==============================================
int addContact()
{
    int errornum, socketDescriptor = socket( PF_INET, SOCK_STREAM, 0 );
    string buffer;
    struct sockaddr_in sa;
    char name[81];

    sa.sin_family = AF_INET;

    cout <<  "Digite o endereço IP:\t";
    cin >> buffer;

    // Conversão da string em endereço IP.
    if ( (errornum = inet_pton( sa.sin_family, buffer.c_str(), &sa.sin_addr )) < 1 )
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
        cout << "Digite um nome para o contato de até 80 caracteres e sem espaços:\t";
        cin >> buffer;

        if ( contactListSearch( contacts, buffer.c_str() ) != NULL ){
            cout << "Nome já utilizado!\n";
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
    ssize_t total = 0, partial, nameCharSize = sizeof( char ) * (serverName.size() + 1);

    // Envia o tamanho do nome do servidor local, em bytes.
    while ( total < sizeof( total ) )
    {
        partial = send( socketDescriptor, (void *)(&nameCharSize + total), sizeof( nameCharSize ) - total, 0 );
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
        partial = send( socketDescriptor, (void *)(serverName.c_str() + total), nameCharSize - total, 0 );
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

    alertMenu( "Contato adicionado com sucesso." );

    return 1;
}

//=================== DELETE CONTACT ===========================================
void deleteContact()
{
    string buffer;

    cout << "Digite o nome do contato, não utilize espaços e máximo de 80 caracteres."  << endl;
    getline( cin, buffer );

    pthread_rwlock_wrlock( &contacts->sync );
    ContactNode *deleted = contactListSearch( contacts, buffer.c_str() );

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
    }
    pthread_rwlock_unlock( &contacts->sync );

    pthread_rwlock_wrlock( &pendingReadSync );
    pendingRead = 0;
    pthread_rwlock_unlock( &pendingReadSync );
}

void messageMenu(){

    int contactId;
    ContactNode* sender;        //De quem se deseja ler as mensagens
    GenericNode* messageNode;   //Mensagem que será lida.

    cout << "Digite o ID do contato para ver as mensagens recebidas:\t";
    cin >> contactId;

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
                cout << "> " << (char *)messageNode->item << endl;
            }
            pthread_rwlock_unlock( &contacts->sync );
        }
    }
}

//=================== SEND MESSAGE =============================================
void sendMessage(){

    int sendResult;
    size_t bufferSize = 81;
    string buffer;
    ContactNode* receiver;

    cout << "Digite o nome do contato:\t";
    getline( cin, buffer );

    pthread_rwlock_wrlock( &contacts->sync );
    receiver = contactListSearch(contacts, buffer.c_str());

    if ( receiver == NULL ){
        pthread_rwlock_unlock( &contacts->sync );
        alertMenu( "Não há contato com esse nome na sua lista de contatos!" );
    }else{
        cout << "Digite sua mensagem:\n";
        getline( cin, buffer );

        sendResult = send( receiver->socket, buffer.c_str(), (buffer.size() + 1) * sizeof(char), 0 );

        if(sendResult == -1){
            alertMenu("Falha no envio!");
            switch ( errno ) {
                case EBADF:
                case ENOTSOCK:
                case ENOTCONN:
                case EPIPE:
                    contactListRemove( contacts, receiver );
                    fprintf( stderr, "O contato não está conectado e foi removido da lista.\n" );
                    break;
                default:
                    perror( "Erro inesperado" );
                    fprintf( stderr, "Tente enviar novamente.\n" );
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
    string buffer;
    string message;
    ContactNode *receiver;

    cout << "Digite a mensagem:\n";
    getline( cin, message );

    cout << "\nInsira os IDs dos contatos para quem deseja enviar a mensagem seguidos de <ENTER>.\n"
          << "Ex: 105 201 110\n";
    getline( cin, buffer );
    stringstream idList( buffer );

    idList >> receiverId;
    pthread_rwlock_rdlock( &contacts->sync );
    if ( !idList ) { // Se não houve erro.
        do {
            //Pesquisa do ID dentro da lista e envio da mensagem.
            receiver = contactListSearchId(contacts, receiverId);
            if(receiver == NULL){
                fprintf( stderr, "ID inválido: %d\n", receiverId );
            }else{
                send( receiver->socket, message.c_str(), (message.size() + 1) * sizeof(char), 0 );
                idList >> receiverId;
            }
        } while( !idList );
    }
    pthread_rwlock_unlock( &contacts->sync );

    alertMenu("Mensagem broadcast enviada!");
}

void acceptContact() {
    pthread_rwlock_wrlock( &pendingAccept->sync );

    if ( pendingAccept->size > 0 ) {
        ContactNode *current = contactListPopFront( pendingAccept );
        string accept, name;

        cout << "Há" << pendingAccept->size << "contatos a serem aceitos.\n";
        while ( current != NULL ) {
            cout << "Nome: " << current->name << "\nAceitar [s/n]?\t";
            cin >> accept;
            if ( accept[0] == 's' ) {
                pthread_rwlock_wrlock( &contacts->sync );

                if ( contactListSearch( contacts, current->name ) != NULL ) {
                    do {
                        cout << "O nome já está na sua lista, digite outro nome com até 80 caracteres.\n";
                        cin >> name;
                        current->name = (char *) realloc( current->name, sizeof(char) * ( name.size() + 1 ) );
                    } while ( contactListSearch( contacts, current->name ) != NULL );
                }
                contactListInsert( contacts, current );

                pthread_rwlock_unlock( &contacts->sync );

                pthread_rwlock_wrlock( &socketSetSync );
                FD_SET( current->socket, &socketSet );
                pthread_rwlock_unlock( &socketSetSync );

                cout << "Contato adicionado com sucesso!\n";
            }
            current = contactListPopFront( pendingAccept );
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
        cout << "=============================================\n\t[ Menu ] - Bem vindo(a) " << serverName << "\n=============================================\n\n";

        pthread_rwlock_wrlock( &pendingReadSync );
        if ( pendingRead == 2 ) {
            cout << ( "Aviso! Há novas mensagens não lidas!\n\n" );
            pendingRead = 1;
        }
        pthread_rwlock_unlock( &pendingReadSync );

        cout << ("[1] - Adicionar contato.\n");
        cout << ("[2] - Listar contatos.\n");
        cout << ("[3] - Deletar contato.\n");
        cout << ("[4] - Enviar mensagem.\n");
        cout << ("[5] - Enviar mensagem em grupo.\n");

        pthread_rwlock_rdlock( &pendingReadSync );
        if ( pendingRead == 1 )
            cout << ("[6] - Ler mensagens.\n");
        pthread_rwlock_unlock( &pendingReadSync );

        pthread_rwlock_rdlock( &pendingAccept->sync );
        if ( pendingAccept->size > 0 ){
            alerts = 1;
            cout << ("[7] - Adições pendentes.\n");
        }
        pthread_rwlock_unlock( &pendingAccept->sync );

        cout << ("[0] - Fechar programa.\n\n=============================================\n");
        cout << ("~$ ");

        cin >> input;

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
