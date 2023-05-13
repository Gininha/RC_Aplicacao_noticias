#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/msg.h>

#define BUFLEN 1024 // Tamanho do buffer
#define MQ_ID 1447

#define USERNAME "Username: \0"
#define PASSWORD "Password: \0"
#define ERRO_AUTENTICACAO "Admin não encontrado\n\0"
#define ERRO_LOGIN "Utilizador não encontrado\n\0"
#define ADD_USER_WRONG "ADD_USER {username} {password} {administrador/cliente/jornalista}\n\0"
#define DEL_USER_WRONG "DEL {username}\n\0"
#define SERVER_FINISH "Servidor Terminado !!!\n\0"
#define SERVER_CONNECT "Admitido\n\0"
#define NO_TOPICS "No Topics Available at the moment!!!\n\0"

typedef struct
{
    char username[100];
    char password[100];
    char role[100];
} REGISTOS;

typedef struct TOPICS
{
    char id[5];
    char Titulo[150];
    struct TOPICS *next;
    int num_topicos;
    int multicast_socket;
    struct sockaddr_in addr;
} TOPICS;

typedef struct MQ
{
    long id;
    char message[512];
}MQ;

TOPICS *topicos;
key_t key;
int shmid, mq_id;
sem_t mutex;

void erro(char *s)
{
    perror(s);
    exit(1);
}

int confirmacao(char *username, char *password, char *ficheiro)
{

    REGISTOS *registos = malloc(sizeof(REGISTOS));
    int keep_reading = 1;

    FILE *f = fopen(ficheiro, "r");
    if (f == NULL)
    {
        return 0;
    }

    do
    {
        if (feof(f))
        {
            keep_reading = 0;
            fclose(f);
            return 0;
        }

        fscanf(f, "%s %s %s", registos->username, registos->password, registos->role);

        if (strcmp(registos->role, "administrador") == 0)
        {
            if (strcmp(registos->username, username) == 0)
            {
                if (strcmp(registos->password, password) == 0)
                {
                    fclose(f);
                    return 1;
                }
            }
        }

    } while (keep_reading);

    return 0;
}

int autenticacao(int s, struct sockaddr_in *si_outra, socklen_t *slen, char *ficheiro)
{

    int rec_len, validacao;
    char username[100];
    char password[100];

    username[0] = '\0';
    password[0] = '\0';

    sendto(s, USERNAME, strlen(USERNAME), 0, (const struct sockaddr *)si_outra, *slen);

    rec_len = recvfrom(s, username, BUFLEN, 0, (struct sockaddr *)si_outra, slen);
    username[rec_len - 1] = '\0';

    sendto(s, PASSWORD, strlen(PASSWORD), 0, (const struct sockaddr *)si_outra, *slen);

    rec_len = recvfrom(s, password, BUFLEN, 0, (struct sockaddr *)si_outra, slen);
    password[rec_len - 1] = '\0';

    validacao = confirmacao(username, password, ficheiro);

    return validacao;
}

void conecao_inicial(int s, struct sockaddr_in *si_outra, socklen_t *slen)
{
    char buffer[10];

    for (int i = 0; i < 5; i++)
    {
        recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr *)si_outra, slen);
    }

    buffer[0] = '\0';
}

void add_user(char *username, char *password, char *tipo)
{

    // printf("Adicionando user %s\n", username);

    FILE *f = fopen("Users.txt", "a");
    if (f == NULL)
    {
        return;
    }

    fprintf(f, "%s %s %s\n", username, password, tipo);

    fclose(f);
}

void del_user(char *username)
{

    // printf("Deletando user %s\n", username);

    FILE *f, *f_temp;

    char username2[100], password[100], tipo[100];

    if ((f = fopen("Users.txt", "r")) == NULL)
    {
        return;
    }

    if ((f_temp = fopen("Temp.txt", "a")) == NULL)
    {
        return;
    }

    while (fscanf(f, "%s %s %s", username2, password, tipo) == 3)
    {

        if (strcmp(username, username2) == 0)
        {
            continue;
        }
        else
        {
            fprintf(f_temp, "%s %s %s\n", username2, password, tipo);
        }
    }

    fclose(f);
    fclose(f_temp);

    remove("Users.txt");
    rename("Temp.txt", "Users.txt");
    return;
}

void list_users(int s, struct sockaddr_in si_outra)
{

    // printf("Users:\n");
    sendto(s, "Users:\n\0", 8, 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));

    int i = 0;
    char username[100], password[100], tipo[100];

    FILE *f = fopen("Users.txt", "r");
    if (f == NULL)
    {
        return;
    }

    while (fscanf(f, "%s %s %s", username, password, tipo) == 3)
    {
        i++;
        // printf("%d -> %s %s %s\n", i, username, password, tipo);
        char string[1024];
        sprintf(string, "%d -> %s %s %s\n", i, username, password, tipo);
        string[strlen(string) + 1] = '\0';
        sendto(s, string, strlen(string), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
        string[0] = '\0';
    }

    fclose(f);

    return;
}

void connect_udp(struct sockaddr_in si_outra, int s, socklen_t slen, char *ficheiro)
{

    char buf[BUFLEN];
    char *client_ip;
    int client_port, validacao = 0;

    while (1)
    {

        if (validacao == 0)
        {
            validacao = autenticacao(s, &si_outra, &slen, ficheiro);
            if (validacao == 0)
            {
                sendto(s, ERRO_AUTENTICACAO, strlen(ERRO_AUTENTICACAO), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                continue;
            }
            else
            {
                client_ip = inet_ntoa(si_outra.sin_addr);
                client_port = ntohs(si_outra.sin_port);
                sendto(s, SERVER_CONNECT, strlen(SERVER_CONNECT), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
            }
        }

        int rec_len;
        char *token;

        rec_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *)&si_outra, (socklen_t *)&slen);
        buf[rec_len - 1] = '\0';

        token = strtok(buf, " ");

        if (client_port == ntohs(si_outra.sin_port))
        {
            if (strcmp(client_ip, inet_ntoa(si_outra.sin_addr)) == 0)
            {
                if (strcmp(token, "ADD_USER") == 0)
                {
                    char username[100];
                    char password[100];
                    char tipo[100];

                    username[0] = '\0';
                    password[0] = '\0';
                    tipo[0] = '\0';

                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                        continue;
                    }
                    strcpy(username, token);

                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                        continue;
                    }
                    strcpy(password, token);

                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                        continue;
                    }
                    strcpy(tipo, token);

                    add_user(username, password, tipo);
                    char string[250];
                    sprintf(string, "%s %s\n", "Adicionando User:", username);
                    string[strlen(string) + 1] = '\0';
                    sendto(s, string, strlen(string), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                }
            }
        }
        if (client_port == ntohs(si_outra.sin_port))
        {
            if (strcmp(client_ip, inet_ntoa(si_outra.sin_addr)) == 0)
            {
                if (strcmp(token, "DEL") == 0)
                {

                    char username[100];

                    username[0] = '\0';

                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        sendto(s, DEL_USER_WRONG, strlen(DEL_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                        continue;
                    }
                    strcpy(username, token);

                    del_user(username);
                    char string[250];
                    sprintf(string, "%s %s\n", "Del. User:", username);
                    string[strlen(string) + 1] = '\0';
                    sendto(s, string, strlen(string), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                }
            }
        }
        if (client_port == ntohs(si_outra.sin_port))
        {
            if (strcmp(client_ip, inet_ntoa(si_outra.sin_addr)) == 0)
            {
                if (strcmp(token, "LIST") == 0)
                {

                    list_users(s, si_outra);
                }
            }
        }

        if (client_port == ntohs(si_outra.sin_port))
        {
            if (strcmp(client_ip, inet_ntoa(si_outra.sin_addr)) == 0)
            {
                if (strcmp(token, "QUIT") == 0)
                {

                    validacao = 0;
                    client_port = 0;
                    client_ip = '\0';
                }
            }
        }

        if (client_port == ntohs(si_outra.sin_port))
        {
            if (strcmp(client_ip, inet_ntoa(si_outra.sin_addr)) == 0)
            {
                if (strcmp(token, "QUIT_SERVER") == 0)
                {

                    sendto(s, SERVER_FINISH, strlen(SERVER_FINISH), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
                    close(s);
                    exit(0);
                }
            }
        }
    }
}

int validate(int client, char *ficheiro, char *my_role)
{
    int nread = 0;
    REGISTOS *registo = malloc(sizeof(REGISTOS)), *utilizador = malloc(sizeof(REGISTOS));
    int keep_reading = 1;

    FILE *f = fopen(ficheiro, "r");

    write(client, USERNAME, 1 + strlen(USERNAME));
    nread = read(client, utilizador->username, BUFLEN - 1);
    utilizador->username[nread - 1] = '\0';

    write(client, PASSWORD, 1 + strlen(PASSWORD));
    nread = read(client, utilizador->password, BUFLEN - 1);
    utilizador->password[nread - 1] = '\0';

    if (f == NULL)
    {
        return 0;
    }

    do
    {
        if (feof(f))
        {
            keep_reading = 0;
            fclose(f);
            return 0;
        }

        fscanf(f, "%s %s %s", registo->username, registo->password, registo->role);

        if (strcmp(registo->username, utilizador->username) == 0)
        {
            if (strcmp(registo->password, utilizador->password) == 0)
            {
                fclose(f);
                strcpy(my_role, registo->role);
                return 1;
            }
        }

    } while (keep_reading);

    return 0;
}

int welcome_message(int client, char *my_role)
{
    char message_login[BUFLEN];
    int permissoes;

    if ((strcmp(my_role, "administrador")) == 0)
        permissoes = 1;
    if ((strcmp(my_role, "jornalista")) == 0)
        permissoes = 2;
    if ((strcmp(my_role, "leitor")) == 0)
        permissoes = 3;

    switch (permissoes)
    {
    case 1:
        sprintf(message_login, "%s %s %s %s", "\n\n<--------------------------------->\nBem vindo ao servidor de noticias.\n\nCom o estatuto de",
                my_role, "tem acesso aos seguintes comandos:\n-> LIST_TOPICS\n-> SUBSCRIBE_TOPIC <id do tópico>\n\n",
                "Atualmente os tópicos existentes são:\n<--------------------------------->\n");
        break;

    case 2:
        sprintf(message_login, "%s %s %s %s", "\n\n<--------------------------------->\nBem vindo ao servidor de noticias.\n\nCom o estatuto de",
                my_role, "tem acesso aos seguintes comandos:\n-> LIST_TOPICS\n-> SUBSCRIBE_TOPIC <id do tópico>\n-> CREATE_TOPIC <id do tópico> <título do tópico>\n-> SEND_NEWS <id do tópico> <noticia>\n\n",
                "Atualmente os tópicos existentes são:\n<--------------------------------->\n");
        break;

    case 3:
        sprintf(message_login, "%s %s %s %s", "\n\n<--------------------------------->\nBem vindo ao servidor de noticias.\n\nCom o estatuto de",
                my_role, "tem acesso aos seguintes comandos:\n-> LIST_TOPICS\n-> SUBSCRIBE_TOPIC <id do tópico>\n\n",
                "Atualmente os tópicos existentes são:\n<--------------------------------->\n");
        break;
    }

    write(client, message_login, strlen(message_login) + 1);

    return permissoes;
}

void create_multicast(TOPICS *tops, int client)
{
    char *ip = malloc(sizeof(char) * (8 + strlen(tops->id)));

    int port = 5000 + atoi(tops->id);
    
    MQ message;

    strcpy(ip, "239.0.0.");
    strcat(ip, tops->id);

    if ((tops->multicast_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    // set up the multicast address structure
    memset(&tops->addr, 0, sizeof(tops->addr));
    tops->addr.sin_family = AF_INET;
    tops->addr.sin_addr.s_addr = inet_addr(ip);
    tops->addr.sin_port = htons(port);

    // set the multicast TTL
    int ttl = 1; // set to 1 to limit the scope to the same subnet
    if (setsockopt(tops->multicast_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }
    char message_client[1054];
    sprintf(message_client, "Topico %s criado com sucesso!!!\n", tops->Titulo);

    write(client, message_client, 1+strlen(message_client));

    while(1){

        if (msgrcv(mq_id, &message, sizeof(MQ) - sizeof(long), atoi(tops->id), 0) < 0) {
            perror("msgrcv");
            exit(1);
        }
        printf("%s\n", message.message);

        
        if (sendto(tops->multicast_socket, message.message, strlen(message.message), 0, (struct sockaddr *)&tops->addr, sizeof(tops->addr)) < 0) {
            perror("sendto");
            exit(1);
        }
        
    }
}

void create_topic(char *title, char *id_topic, int client)
{

    sem_wait(&mutex);
    TOPICS *aux = topicos;
    if (topicos->num_topicos == 0)
    {
        strcpy(topicos->id, id_topic);
        strcpy(topicos->Titulo, title);
        topicos->next = NULL;
        create_multicast(topicos, client);
    }
    else
    {
        while (aux->next != NULL)
        {
            aux = aux->next;
        }

        aux->next = (TOPICS *)malloc(sizeof(TOPICS));
        if (aux->next == NULL)
        {
            perror("malloc");
            exit(1);
        }

        strcpy(aux->next->id, id_topic);
        strcpy(aux->next->Titulo, title);
        aux->next->next = NULL;
        create_multicast(topicos, client);
    }
    topicos->num_topicos++;
    sem_post(&mutex);
}

void process_client(int client, char *my_role)
{

    int nread = 0;
    char buffer[BUFLEN];
    char id_topic[BUFLEN], title[BUFLEN], news[BUFLEN] = "";
    char *token;
    int permissoes = welcome_message(client, my_role);
    MQ message;

    while (1)
    {

        nread = read(client, buffer, BUFLEN - 1); // Ler mensagem vinda do cliente
        buffer[nread - 1] = '\0';

        token = strtok(buffer, " ");

        if (strcmp(token, "LIST_TOPICS") == 0)
        {
            char temp[200];
            char envio[BUFLEN];
            envio[0] = '\0';

            printf("List:\n");
            TOPICS *aux = topicos;
            int i = 1;
            if(aux->num_topicos == 0){
                write(client, NO_TOPICS, strlen(NO_TOPICS));
                printf("Entrei\n");
                continue;
            }
            while (aux != NULL)
            {
                printf("Entrei_2\n");
                sprintf(temp, "Topico[%d]-> Id: %s\tTitle: %s\n", i, aux->id, aux->Titulo);
                strcat(envio, temp);
                aux = aux->next;
                i++;
            }
            envio[strlen(envio)+1] = '\0';

            write(client, envio, strlen(envio));
        }

        if (strcmp(token, "CREATE_TOPIC") == 0)
        {
            if ((token = strtok(NULL, " ")) == 0)
            {
                printf("CREATE_TOPIC <id do tópico> <título do tópico>\n");
                continue;
            }
            strcpy(id_topic, token);

            if ((token = strtok(NULL, " ")) == 0)
            {
                printf("CREATE_TOPIC <id do tópico> <título do tópico>\n");
                continue;
            }
            strcpy(title, token);
            if (fork() == 0)
            {
                create_topic(title, id_topic, client);
            }
            
        }

        if (strcmp(token, "SEND_NEWS") == 0)
        {
            if ((token = strtok(NULL, " ")) == NULL)
            {
                printf("SEND_NEWS <id do tópico> <noticia>\n");
                continue;
            }
            strcpy(id_topic, token);
            
            // Reset the news string
            strcpy(news, "");

            while ((token = strtok(NULL, " ")) != NULL)
            {
                strcat(news, token);
                strcat(news, " ");
            }
            message.id = atoi(id_topic);
            strcpy(message.message, news);
               
            if (msgsnd(mq_id, &message, sizeof(MQ) - sizeof(long), 0) < 0) {
                perror("msgsnd");
                exit(1);
            }
        }

        buffer[0] = '\0';
    }
}

void init()
{

    key = ftok("shared_memory_key", 1234);

    if ((shmid = shmget(key, sizeof(TOPICS), IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(1);
    }

    if ((topicos = (TOPICS *)shmat(shmid, NULL, 0)) == (TOPICS *)-1)
    {
        perror("shmat");
        exit(1);
    }

    topicos->num_topicos = 0;
    topicos->next = NULL;

    // Initialize the semaphore
    if (sem_init(&mutex, 1, 1) != 0)
    {
        perror("sem_init");
        exit(1);
    }

    //Criacao Message Queue;
    if((mq_id = msgget(MQ_ID, IPC_CREAT|0777))<0){
        perror("msgget");
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in si_minha, si_outra;
    struct sockaddr_in server_tcp, client_tcp;

    int s, ConecaoInicial = 1;
    int s_tcp, client;
    socklen_t slen = sizeof(si_outra);
    socklen_t slen_tcp = sizeof(client_tcp);

    init();

    char my_role[20] = {0}; // initialize my_role to zero

    if (argc != 4)
    {
        printf("./news_server {PORTO_NOTICIAS} {PORTO_CONFIG} {ficheiro configuração}\n");
        return 0;
    }

    if (fork() == 0)
    {
        // Cria um socket para recepção de pacotes UDP
        if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        {
            erro("Erro na criação do socket");
        }

        // Preenchimento da socket address structure
        si_minha.sin_family = AF_INET;
        si_minha.sin_port = htons(atoi(argv[2]));
        si_minha.sin_addr.s_addr = htonl(INADDR_ANY);

        // Associa o socket à informação de endereço
        if (bind(s, (struct sockaddr *)&si_minha, sizeof(si_minha)) == -1)
        {
            erro("Erro no bind");
        }

        if (ConecaoInicial)
        {
            conecao_inicial(s, &si_outra, &slen);
        }

        connect_udp(si_outra, s, slen, argv[3]);
    }
    s_tcp = socket(AF_INET, SOCK_STREAM, 0);

    int optval = 1;
    if (setsockopt(s_tcp, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_tcp.sin_family = AF_INET;
    server_tcp.sin_port = htons(atoi(argv[1]));
    server_tcp.sin_addr.s_addr = INADDR_ANY;

    bind(s_tcp, (struct sockaddr *)&server_tcp, sizeof(server_tcp));

    listen(s_tcp, 10);

    while (1)
    {

        while (waitpid(-1, NULL, WNOHANG) > 0)
            ;

        client = accept(s_tcp, (struct sockaddr *)&client_tcp, (socklen_t *)&slen_tcp);

        if (client > 0)
        {
            if (fork() == 0)
            {
                if (validate(client, argv[3], my_role))
                {
                    process_client(client, my_role);
                }
                else
                    write(client, ERRO_LOGIN, strlen(ERRO_LOGIN));
                close(client);
                exit(0);
            }
        }
    }
}
