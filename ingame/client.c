// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <pthread.h>
// #include <arpa/inet.h>  // inet_pton 사용을 위해 추가

// #define BUF_SIZE 1024
// #define PORT 8080
// #define SERVER_IP "127.0.0.1"  // 서버 IP 주소

// // 사용자 구조체 정의 (소켓과 닉네임을 저장)
// typedef struct {
//     int sock;
//     char nickname[BUF_SIZE];
// } Client;

// Client client;  // 클라이언트 정보

// void error(char *msg) {
//     perror(msg);
//     exit(1);
// }

// void *receive_messages(void *sockfd) {
//     int sock = *(int *)sockfd;
//     char msg[BUF_SIZE];
//     int len;

//     while (1) {
//         len = read(sock, msg, sizeof(msg) - 1);
//         if (len <= 0) {
//             break;  // 서버 연결이 끊어졌을 경우
//         }
//         msg[len] = '\0';  // 메시지 종료 처리
//         printf("%s", msg);  // 받은 메시지 출력
//     }
//     return NULL;
// }

// void vote(int sockfd) {
//     char vote_msg[BUF_SIZE];
//     int vote_target;

//     printf("투표할 플레이어의 번호를 입력하세요 (1~5): ");
//     fgets(vote_msg, sizeof(vote_msg), stdin);

//     // 입력값을 정수로 변환하여 투표 대상 지정
//     if (sscanf(vote_msg, "%d", &vote_target) == 1 && vote_target >= 1 && vote_target <= 5) {
//         snprintf(vote_msg, sizeof(vote_msg), "!vote %d", vote_target);
//         write(sockfd, vote_msg, strlen(vote_msg));
//     } else {
//         printf("잘못된 입력입니다. 1부터 5까지의 번호를 입력하세요.\n");
//     }
// }

// int main() {
//     int sockfd;
//     struct sockaddr_in server_addr;
//     pthread_t recv_thread;
//     char msg[BUF_SIZE];

//     // 소켓 생성
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) {
//         error("socket failed");
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);

//     // IP 주소 변환: inet_pton 사용
//     if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
//         error("inet_pton failed");
//     }

//     // 서버에 연결
//     if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         error("connect failed");
//     }

//     printf("서버에 연결되었습니다.\n");

//     // 메시지 수신을 위한 스레드 생성
//     pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd);

//     // 게임 참여 시 닉네임 입력 받기
//     printf("게임에 참여합니다. 닉네임을 입력하세요: ");
//     fgets(client.nickname, sizeof(client.nickname), stdin);
//     client.nickname[strcspn(client.nickname, "\n")] = '\0';  // 개행 문자 제거
//     write(client.sock, client.nickname, strlen(client.nickname)); // 서버에 닉네임 전송

//     pthread_create(&recv_thread, NULL, receive_messages, NULL);  // 메시지 수신 스레드
//     while (1) {
//         // 메시지 입력 받기
//         printf("메시지를 입력하세요 (exit - 종료): ");
//         fgets(msg, sizeof(msg), stdin);

//         // 메시지가 "exit"이면 서버와의 연결을 종료하고 종료
//         if (strncmp(msg, "exit", 4) == 0) {
//             break;
//         }

//         // 채팅 메시지 처리
//         if (strncmp(msg, "!vote", 5) == 0) {
//             vote(sockfd);  // 투표 기능 처리
//         } else {
//             // 일반 채팅 메시지 처리
//             write(sockfd, msg, strlen(msg));
//         }
//     }

//     close(sockfd);
//     return 0;
// }
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
        // 클라이언트로부터 입력을 받아 서버로 전송
        char msg[BUF_SIZE];
        fgets(msg, sizeof(msg), stdin);
        write(client.sock, msg, strlen(msg));
    }

    pthread_join(recv_thread, NULL);
    close(client.sock);
    return 0;
}
