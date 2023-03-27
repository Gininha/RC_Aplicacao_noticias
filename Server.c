#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFLEN 512	// Tamanho do buffer
#define PORT 9876	// Porto para recepção das mensagens

#define USERNAME "Username: "
#define PASSWORD "Password: "
#define ERRO_AUTENTICACAO "Admin não encontrado\n"
#define ADD_USER_WRONG "ADD_USER {username} {password} {administrador/cliente/jornalista}\n"
#define DEL_USER_WRONG "DEL {username}\n"

typedef struct{
    char username[100];
    char password[100];
}REGISTOS;

void erro(char *s) {
	perror(s);
	exit(1);
}

int confirmacao(char *username, char *password){

    REGISTOS *registos = malloc(sizeof(REGISTOS));
    int keep_reading = 1;

    FILE *f = fopen("Admins.txt", "r");
    if(f == NULL){
        return 0;
    }

    do{
        if(feof(f)){
            keep_reading = 0;
            fclose(f);
            return 0;
        }
        
        fscanf(f, "%s %s", registos->username, registos->password);

        if(strcmp(registos->username, username) == 0){
            if(strcmp(registos->password, password) == 0){
                fclose(f);
                return 1;
            }
        }

    }while(keep_reading);

    return 0;
}

int autenticacao(int s, struct sockaddr_in* si_outra, socklen_t * slen){
    
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

void conecao_inicial(int s, struct sockaddr_in* si_outra, socklen_t * slen, char *buffer){
    for(int i = 0; i<5; i++){
        recvfrom(s, buffer, BUFLEN, 0, (struct sockaddr*)si_outra, slen);
    }

    buffer[0] = '\0';
}

void add_user(char *username, char *password, char *tipo){

    printf("Adicionando user %s\n", username);

    FILE *f = fopen("Users.txt", "a");
    if(f == NULL){
        return;
    }

    fprintf(f, "%s %s %s\n", username, password, tipo);

    fclose(f);

}

void del_user(char *username){
    printf("Deletando user %s\n", username);

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

void list_users(){

    printf("Users:\n");
    
    int i=0;
    char username[100], password[100], tipo[100];

    FILE *f = fopen("Users.txt", "r");
    if(f == NULL){
        return;
    }

    while(fscanf(f, "%s %s %s", username, password, tipo) == 3){
        i++;
        printf("%d -> %s %s %s\n", i, username, password, tipo);
    }

    fclose(f);

    return;
}

int main(void){
    struct sockaddr_in si_minha, si_outra;

	int s, validacao, ConecaoInicial = 1;
	socklen_t slen = sizeof(si_outra);
	char buf[BUFLEN];


    do{
        // Cria um socket para recepção de pacotes UDP
        if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
            erro("Erro na criação do socket");
        }

        // Preenchimento da socket address structure
        si_minha.sin_family = AF_INET;
        si_minha.sin_port = htons(PORT);
        si_minha.sin_addr.s_addr = htonl(INADDR_ANY);

        // Associa o socket à informação de endereço
        if(bind(s,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
            erro("Erro no bind");
        }
        
        if(ConecaoInicial){
            conecao_inicial(s, &si_outra, &slen, buf);
            ConecaoInicial = 0;
        }

        validacao = autenticacao(s, &si_outra, &slen);

        if(validacao){

            int rec_len;
            char *token;

            while(validacao){
                rec_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen);
                buf[rec_len-1] = '\0';

                token = strtok(buf, " ");

                if(strcmp(token, "ADD_USER") == 0){
                    char username[100];
                    char password[100];
                    char tipo[100];

                    username[0] = '\0';
                    password[0] = '\0';
                    tipo[0] = '\0';

                    token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra)); continue;}
                    strcpy(username, token);

                    token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra)); continue;}
                    strcpy(password, token);

                    token = strtok(NULL, " ");if(token == NULL){sendto(s, ADD_USER_WRONG, strlen(ADD_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra)); continue;}
                    strcpy(tipo, token);

                    add_user(username, password, tipo);
                }

                if(strcmp(token, "DEL") == 0){

                    char username[100];

                    username[0] = '\0';

                    token = strtok(NULL, " ");if(token == NULL){sendto(s, DEL_USER_WRONG, strlen(DEL_USER_WRONG), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra)); continue;}
                    strcpy(username, token);

                    del_user(username);

                }

                if(strcmp(token, "LIST") == 0){
                    
                    list_users();

                }

                if(strcmp(token, "QUIT") == 0){

                    close(s);
                    validacao = 0;
                    
                }

                if(strcmp(token, "QUIT_SERVER") == 0){

                    close(s);
                    exit(0);

                }

            }

        }else{
            sendto(s, ERRO_AUTENTICACAO, strlen(ERRO_AUTENTICACAO), 0, (const struct sockaddr *)&si_outra, sizeof(si_outra));
            close(s);
        }

    }while(1);
    
}