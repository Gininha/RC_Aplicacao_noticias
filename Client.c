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

#define BUFLEN 1024	// Tamanho do buffer

void erro(char *msg) {
  printf("Erro: %s\n", msg);
	exit(-1);
}

void subscribe_topic(char *id){
    struct sockaddr_in addr;
    int sock;
    char *ip = malloc(sizeof(char) * (8+strlen(id)));
    char msg[BUFLEN];
    socklen_t addrlen = sizeof(addr);
    int port = 5000 + atoi(id);

    strcpy(ip, "239.0.0.");
    strcat(ip, id);

    // create a UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // set up the multicast address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // bind the socket to the port
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    // join the multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    int nbytes;

    while(1){
        if ((nbytes = recvfrom(sock, msg, sizeof(msg), 0, (struct sockaddr *)&addr, &addrlen)) < 0) {
            perror("recvfrom");
            exit(1);
        }
        printf("NEWS!!!\n-> %s\n", msg);
    }
}

void login(int fd){
    int nread;
    char buffer[BUFLEN];

    nread = read(fd, buffer, BUFLEN-1);     //Leitura da mensagem inicial de conecçao ao sv
    buffer[nread] = '\0';
    printf("%s", buffer);

    scanf("%s", buffer);
    write(fd, buffer, 1 + strlen(buffer)); 

    nread = read(fd, buffer, BUFLEN-1);     //Leitura da mensagem inicial de conecçao ao sv
    buffer[nread] = '\0';
    printf("%s", buffer);

    scanf("%s", buffer);
    write(fd, buffer, 1 + strlen(buffer)); 
}

int main(int argc, char *argv[]){
    char endServer[100];
    char buffer[BUFLEN], msg[BUFLEN];
    int fd;
    int nread = 0;
    struct sockaddr_in addr;
    struct hostent *hostPtr;
    char id_topic[BUFLEN];
    char *token;


    if (argc != 3) {
        printf("./news_client <ip> <port>\n");
        exit(-1);
    }

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
        erro("Não consegui obter endereço");

    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short) atoi(argv[2]));

    if ((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
        erro("socket");
    if (connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
        erro("Connect");


    login(fd);

    nread = read(fd, buffer, BUFLEN-1);     //Leitura da mensagem inicial de conecçao ao sv
    buffer[nread-1] = '\0';
    printf("%s\n", buffer);

    while(1){
        
        scanf(" %[^\n]", buffer);
        strcpy(msg, buffer);
        token = strtok(buffer, " ");
  
        if(strcmp(token, "LIST_TOPICS") == 0){
            write(fd, msg, 1 + strlen(msg));    
        }


        if(strcmp(token, "SUBSCRIBE_TOPIC") == 0){
            if((token = strtok(NULL, " ")) == 0){
                printf("SUBSCRIBE_TOPIC <id do tópico>\n");
                continue;
            }
            strcpy(id_topic, token);
            if(fork() == 0){
                subscribe_topic(id_topic);
            }
            printf("SUBSCRIBED\n");
        }


        if(strcmp(token, "CREATE_TOPIC") == 0){
            write(fd, msg, 1 + strlen(msg));
        }


        if(strcmp(token, "SEND_NEWS") == 0){
            write(fd, msg, 1 + strlen(msg));
        }

        if(strcmp(token, "EXIT") == 0){
            close(fd);
            exit(0);
        }
    }
}
  
