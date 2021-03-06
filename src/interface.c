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
    do
    {
        cout << "Digite um nome para o contato de até 80 caracteres e sem espaços:\t";
        cin >> buffer;

        if ( contactListSearch( contacts, buffer.c_str() ) != NULL ){
            cout << "Nome já utilizado!\n";
        }else
            notUsed = 1;
    } while ( notUsed == 0 ); // Enquanto o nome fornecido já tiver sido utilizado.

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
    FD_SET( socketDescriptor, &socketSet );

    contactListInsert( contacts, contactNodeCreate( socketDescriptor, buffer ) );

    alertMenu( "Contato adicionado com sucesso." );

    return 1;
}

//=================== DELETE CONTACT ===========================================
void deleteContact()
{
    string buffer;

    cout << "Digite o nome do contato, não utilize espaços e máximo de 80 caracteres."  << endl;
    cin >> buffer;

    ContactNode *deleted = contactListSearch( contacts, buffer.c_str() );

    if ( deleted == NULL ){
        alertMenu("Não há contato com esse nome na sua lista de contatos.");
    }else
    {
        FD_CLR( deleted->socket, &socketSet );

        contactListRemove( contacts, deleted );

        alertMenu("Contato removido com sucesso.");
    }

}

//=================== LIST CONTACTS ============================================
void listContact()
{

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
}

void messageMenu(){

    string contactId;
    ContactNode* sender;        //De quem se deseja ler as mensagens
    GenericNode* messageNode;   //Mensagem que será lida.

    cout << "Digite o ID do contato para ver as mensagens recebidas:\t";
    cin >> contactId;

    sender = contactListSearchId( contacts, stoi(contactId) );

    if( sender == NULL ) {
        alertMenu("ID de contato não encontrado!");
    }
    else {
        if(sender->messages->head == NULL){    //Não há mensagens a serem lidas
            alertMenu("Não há mensagens a serem lidas para este contato!");
        }
        else {
            while(sender->messages->head != NULL){ //Verifica se existem msgs a serem lidas
                messageNode = dequePopFront(sender->messages);
                cout << "> " << (char *)messageNode->item << endl;
            }
        }
    }
}

//=================== SEND MESSAGE =============================================
void sendMessage(){

    int sendResult;
    string buffer;
    ContactNode* receiver;

    cout << "Digite o nome do contato:\t";
    cin >> buffer;

    receiver = contactListSearch(contacts, buffer.c_str());

    if ( receiver == NULL ){
        alertMenu( "Não há contato com esse nome na sua lista de contatos!" );
    }else{
        cout << "Digite sua mensagem:\n";
        cin.ignore(1);
        getline(cin, buffer);

        sendResult = send( receiver->socket, buffer.c_str(), (buffer.size() + 1) * sizeof(char), 0 );
        cout << "Bytes a enviar: " <<  sendResult << endl;

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
        }else{
            alertMenu("Mensagem enviada!");
        }
    }
}

//=================== BROADCAST MESSAGE ========================================
void broadcastMessage(){

    int receiverId;
    string buffer;
    string message;
    ContactNode *receiver;

    cout << "Digite a mensagem:\n";
    cin.ignore( 1 );
    getline( cin, message );

    cout << "\nInsira o ID do próximo contato para quem deseja enviar a mensagem seguido de <ENTER> ou * para sair.\n";
    cin >> buffer;

    if ( buffer[0] != '*' ) { // Se não houve erro.
        do {
            receiverId = stoi(buffer);
            //Pesquisa do ID dentro da lista e envio da mensagem.
            receiver = contactListSearchId(contacts, receiverId);
            if(receiver == NULL){
                fprintf( stderr, "ID inválido: %d\n", receiverId );
            }
            else
                send( receiver->socket, message.c_str(), (message.size() + 1) * sizeof(char), 0 );
            cout << "\nInsira o ID do próximo contato para quem deseja enviar a mensagem seguido de <ENTER> ou * para sair.\n";
            cin >> buffer;
        } while( buffer[0] != '*' );
    }

    alertMenu("Mensagem broadcast enviada!");
}

void acceptContact() {

    if ( pendingAccept->size > 0 ) {
        cout << "Há " << pendingAccept->size << " contatos a serem aceitos.\n";
        ContactNode *current = contactListPopFront( pendingAccept );
        string accept, name;
        while ( current != NULL ) {
            cout << "Nome: " << current->name << "\nAceitar [s/n]?\t";
            cin >> accept;
            if ( accept[0] == 's' ) {

                if ( contactListSearch( contacts, current->name ) != NULL ) {
                    do {
                        cout << "O nome já está na sua lista, digite outro nome com até 80 caracteres.\n";
                        cin >> name;
                        current->name = (char *) realloc( current->name, sizeof(char) * ( name.size() + 1 ) );
                        strcpy( current->name,  name.c_str() );
                    } while ( contactListSearch( contacts, current->name ) != NULL );
                }
                contactListInsert( contacts, current );


                FD_SET( current->socket, &socketSet );

                cout << "Contato adicionado com sucesso!\n";
            }
            current = contactListPopFront( pendingAccept );
        }
    }
    alertMenu( "Não há mais contatos a serem aceitos." );
}

//=================== MAIN MENU ================================================
void menu(){

    int alerts;
    string input;

    do{
        alerts = 0;
        cout << "=============================================\n\t[ Menu ] - Bem vindo(a) " << serverName << "\n=============================================\n\n";

        if ( pendingRead == 1 ) {
            cout << ( "Aviso! Há novas mensagens não lidas!\n\n" );
            pendingRead = 0;
        }

        cout << ("\t[1] - Adicionar contato.\n");
        cout << ("\t[2] - Listar contatos.\n");
        cout << ("\t[3] - Deletar contato.\n");
        cout << ("\t[4] - Enviar mensagem.\n");
        cout << ("\t[5] - Enviar mensagem em grupo.\n");

        cout << ("\t[6] - Ler mensagens.\n");

        if ( pendingAccept->size > 0 ){
            alerts = 1;
            cout << ("\t[7] - Adições pendentes.\n");
        }

        cout << ("\t[0] - Fechar programa.\n\n=============================================\n");
        cout << ("~$ ");

        cin >> input;

        switch(input[0]) {
          	case '1':
          	   alertMenu("> Adicionar contato");
        	    addContact();
      		    break;
      	    case '2':
      	        alertMenu("> Listar contatos");
        		listContact();
        		break;
          	case '3':
          	    alertMenu("> Deletar contato");
        		deleteContact();
        		break;
            case '4':
                alertMenu("> Envio de mensagem");
                sendMessage();
      		    break;
            case '5':
                alertMenu("> Envio de mensagem em grupo");
                broadcastMessage();
      		    break;
      		case '6':
                alertMenu("> Leitura de mensagens");
      		    messageMenu();
      		    break;
            case '7':
                if ( alerts == 0 )
                    alertMenu("Opção inválida! Tente novamente.");
                else {
                    alertMenu("> Aceitar contatos");
                    acceptContact();
                }
      		    break;
            case '0':
                break;
      	    default:
                alertMenu("Opção inválida! Tente novamente.");
        	    break;
      	}
    }while(input[0] != '0');
}
