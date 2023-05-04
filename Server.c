#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFLEN 512	// Tamanho do buffer
#define PORT_CONFIG 9876	// Porto para recepção das mensagens
#define PORT_NOTICIAS 9000

#define USERNAME "Username: \0"
#define PASSWORD "Password: \0"
#define ERRO_AUTENTICACAO "Admin não encontrado\n\0"
#define ADD_USER_WRONG "ADD_USER {username} {password} {administrador/cliente/jornalista}\n\0"
#define DEL_USER_WRONG "DEL {username}\n\0"
#define SERVER_FINISH "Servidor Terminado !!!\n\0"
#define SERVER_CONNECT "Admitido\n\0"

typedef struct{
    char username[100];
    char password[100];
}REGISTOS;

//Connection flag: whether a connection is still active
int ConecaoInicial = 1;
//Validation flag: whether a validation resulted in a match
int validacao = 0;


void erro(char *s) {
    perror(s);
    exit(1);
}


void conecao_inicial(int s, struct sockaddr_in* si_outra, socklen_t * slen, char *buffer){
    for(int i = 0; i<5; i++)
    {
        recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr*)si_outra, slen);
    }
    buffer[0] = '\0';
}

int confirmacao(char *username, char *password)
{

    REGISTOS *registos = malloc(sizeof(REGISTOS));
    int keep_reading = 1;

    FILE *f = fopen("Admins.txt", "r");
    if(f == NULL)
    {
        return 0;
    }
    do
    {
        if(feof(f))
        {
            keep_reading = 0;
            fclose(f);
        }

        fscanf(f, "%s %s", registos->username, registos->password);

        if(strcmp(registos->username, username) == 0)
        {
            if(strcmp(registos->password, password) == 0)
            {
                fclose(f);
                return 1;
            }
        }

    }while(keep_reading);

    return 0;
}

int autenticacao(int s, struct sockaddr_in* si_outra, socklen_t *slen){

    int rec_len, validacao;
    char username[100];
    char password[100];

    username[0] = '\0';
    password[0] = '\0';


    sendto(s, USERNAME, strlen(USERNAME), 0, (const struct sockaddr*)si_outra, *slen);

    rec_len = recvfrom(s, username, BUFLEN, 0, (struct sockaddr*)si_outra, slen);
    username[rec_len-1] = '\0';

    sendto(s, PASSWORD, strlen(PASSWORD), 0, (const struct sockaddr*)si_outra, *slen);

    rec_len = recvfrom(s, password, BUFLEN, 0, (struct sockaddr*)si_outra, slen);
    password[rec_len-1] = '\0';

    validacao = confirmacao(username, password);

    return validacao;
}

void add_user(char *username, char *password, char *tipo){

    //printf("Adicionando user %s\n", username);

    FILE *f = fopen("Users.txt", "a");
    if(f == NULL)
    {
        return;
    }

    fprintf(f, "%s %s %s\n", username, password, tipo);
    fclose(f);

}

void del_user(char *username){

    //printf("Deletando user %s\n", username);

    FILE *f, *f_temp;

    char username2[100], password[100], tipo[100];

    if((f = fopen("Users.txt", "r")) == NULL){
        return;
    }

    if((f_temp = fopen("Temp.txt", "a")) == NULL){
        return;
    }

    while(fscanf(f, "%s %s %s", username2, password, tipo) == 3){

        if(strcmp(username, username2) == 0){
            continue;
        }else{
            fprintf(f_temp, "%s %s %s\n", username2, password, tipo);
        }
    }

    fclose(f);
    fclose(f_temp);

    remove("Users.txt");
    rename("Temp.txt", "Users.txt");
    return;
}

void list_users(int s, struct sockaddr_in si_outra){

    //printf("Users:\n");
    sendto(s, "Users:\n\0", 8, 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));

    int i=0;
    char username[100], password[100], tipo[100];

    FILE *f = fopen("Users.txt", "r");
    if(f == NULL){
        return;
    }

    while(fscanf(f, "%s %s %s", username, password, tipo) == 3){
        i++;
        //printf("%d -> %s %s %s\n", i, username, password, tipo);
        char string[1024];
        sprintf(string, "%d -> %s %s %s\n", i, username, password, tipo);
        string[strlen(string)+1] = '\0';
        sendto(s, string, strlen(string), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
        string[0] = '\0';

    }

    fclose(f);
    return;
}

void UDP_CONNECTION(int s, struct sockaddr_in* si_outra, socklen_t * slen, char *buffer,const char *client_ip, int client_port)
{
    //No connection established yet
    if(ConecaoInicial)
    {
        conecao_inicial(s, si_outra, slen, buffer);
        //Connection established
        ConecaoInicial = 0;
    }
    while(1)
    {
        if(validacao == 0)
        {
            validacao = autenticacao(s, si_outra, slen);
            if(validacao == 0)
            {
                sendto(s, ERRO_AUTENTICACAO, strlen(ERRO_AUTENTICACAO), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra));
                validacao = 0;
                client_port = 0;
                client_ip = '\0';
                break;
            }
            else
            {
                client_ip = inet_ntoa(si_outra->sin_addr);
                client_port = ntohs(si_outra->sin_port);
                sendto(s, SERVER_CONNECT, strlen(SERVER_CONNECT), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra));

                //ADMIN INTERFACE
                while(1)
                {
                    int rec_len;
                    char *token;
                    rec_len = recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr *) si_outra, (socklen_t *)&slen);
                    buffer[rec_len-1] = '\0';
                    if(client_port == ntohs(si_outra->sin_port))
                    {
                        if(strcmp(client_ip, inet_ntoa(si_outra->sin_addr)) == 0)
                        {
                            if(strcmp(token, "ADD_USER") == 0)
                            {
                                char username[100];
                                char password[100];
                                char tipo[100];

                                username[0] = '\0';
                                password[0] = '\0';
                                tipo[0] = '\0';

                                token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra)); continue;}
                                strcpy(username, token);

                                token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra)); continue;}
                                strcpy(password, token);

                                token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra)); continue;}
                                strcpy(tipo, token);

                                add_user(username, password, tipo);
                                char string[250];
                                sprintf(string, "%s %s\n", "Adicionando User:", username);
                                string[strlen(string)+1] = '\0';
                                sendto(s, string, strlen(string), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra));
                            }
                            else if(strcmp(token, "DEL") == 0)
                            {
                                char username[100];
                                username[0] = '\0';
                                token = strtok(NULL, " ");if(token == NULL){sendto(s, DEL_USER_WRONG, strlen(DEL_USER_WRONG), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra)); continue;}
                                strcpy(username, token);

                                del_user(username);
                                char string[250];
                                sprintf(string, "%s %s\n", "Del. User:", username);
                                string[strlen(string)+1] = '\0';
                                sendto(s, string, strlen(string), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra));

                            }
                            else if(strcmp(token, "LIST") == 0){

                                list_users(s, *si_outra);

                            }
                            else if(strcmp(token, "QUIT") == 0){

                                validacao = 0;
                                client_port = 0;
                                client_ip = '\0';

                            }
                            else if(strcmp(token, "QUIT_SERVER") == 0){

                                sendto(s, SERVER_FINISH, strlen(SERVER_FINISH), 0, (const struct sockaddr *)si_outra, sizeof(*si_outra));
                                close(s);
                                exit(0);

                            }
                        }
                    }
                }


            }
        }
    }

}

void TCP_CLIENT(int fd_client)
{
    int nread = 0;
    char buffer[BUFLEN];
    do
    {
        write(fd_client,"Bem vindo!\nIntroduza comando: \n",BUFLEN-1);
        nread = read(fd_client, buffer, BUFLEN-1);

        buffer[nread] = '\0';

        printf("%s\n", buffer);
        fflush(stdout);
    } while (nread>0);

    close(fd_client);
}










int main(void)
{
    int fd,s,client;
    struct sockaddr_in si_minha, si_outra;
    struct sockaddr_in si_tcp_server,si_tcp_client;
    socklen_t slen = sizeof(si_outra);
    char buf[BUFLEN];
    char *client_ip;
    int client_port = 0;


    si_tcp_server.sin_family = AF_INET;
    si_tcp_server.sin_addr.s_addr = htonl(INADDR_ANY);
    si_tcp_server.sin_port = htons(PORT_NOTICIAS);


    // Preenchimento da socket address structure
    si_minha.sin_family = AF_INET;
    si_minha.sin_addr.s_addr = htonl(INADDR_ANY);
    si_minha.sin_port = htons(PORT_CONFIG);




    // Creating socket for TCP packets reception
    if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("Erro na criação de socket para TCP");


    // Creating socket for UDP packets reception
    if( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        erro("Erro na criação de socket para UDP");
    }

    // Binding TCP socket to server address
    if(bind(fd,(struct sockaddr*)&si_tcp_server, sizeof(si_tcp_server)) == -1) {
        erro("Erro no bind do socket TCP");
    }

    // Binding UDP socket to server address
    if(bind(s,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
        erro("Erro no bind do socket UDP");
    }

    //Creating a child process specific for UDP admin work

    if(fork() == 0)
    {
        UDP_CONNECTION(s, &si_outra, &slen, buf,client_ip,client_port);
    }

    //Father process deals with TCP clients
    //p.e Server max lotation is 10 clients
    //Server stays active until SIGINT or an admin command

    //pid_t tcp_clients[10];
    while(1)
    {
        while (waitpid(-1, NULL, WNOHANG) > 0);
        client = accept(fd, (struct sockaddr *) &si_outra, (socklen_t *) &slen);

        if (client > 0)          //TCP connection accepted with a client
        {
            if (fork() == 0)     //Creating child process to deal with client
            {
                TCP_CLIENT(client);
                close(client);
            }
        }

    }

}
