#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define PORT 8080

typedef struct {
    int sock;
    char nickname[50];
} Client;

Client client;
int voting_mode = 0;  // 투표 모드를 추적하는 변수 (0: 비활성화, 1: 활성화)

void error(char *msg) {
    perror(msg);
    exit(1);
}

void *receive_messages(void *arg) {
    char msg[BUF_SIZE];
    int len;

    while (1) {
        len = read(client.sock, msg, sizeof(msg));
        if (len > 0) {
            msg[len] = '\0';
            printf("%s\n", msg);  // 메시지 출력
        }
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error("Socket error");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Connect error");
    }

    client.sock = sock;
    printf("게임에 참여합니다. 닉네임을 입력하세요: ");
    fgets(client.nickname, sizeof(client.nickname), stdin);
    client.nickname[strcspn(client.nickname, "\n")] = '\0';  
    write(client.sock, client.nickname, strlen(client.nickname));

    pthread_create(&recv_thread, NULL, receive_messages, NULL);  // 메시지 수신 스레드

    while (1) {
        char msg[BUF_SIZE];
        fgets(msg, sizeof(msg), stdin);

        if (voting_mode) {  // 투표 모드일 때
            int vote = atoi(msg);
            if (vote >= 1 && vote <= 5) {
                snprintf(msg, sizeof(msg), "%d\n", vote);  // 서버에 보낼 메시지로 정수 변환
                write(client.sock, msg, strlen(msg));
            } else {
                printf("잘못된 투표 번호입니다. 1~5 사이의 숫자를 입력하세요.\n");
            }
        } else {  // 일반 채팅 모드일 때
            write(client.sock, msg, strlen(msg));
        }
    }

    pthread_join(recv_thread, NULL);
    close(client.sock);
    return 0;
}
