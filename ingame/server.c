#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define MAX_USERS 5
#define BUF_SIZE 1024
#define PORT 8080

typedef struct {
    int sock;
    char nickname[50];
    int role;  
    int voted_for;
} Client;

Client clients[MAX_USERS];
int num_clients = 0;
time_t start_time; 
pthread_mutex_t lock;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void *client_handler(void *arg) {
    Client *client = (Client *)arg;
    char msg[BUF_SIZE];
    int len;

    // 클라이언트로부터 닉네임 받기
    len = read(client->sock, msg, sizeof(msg) - 1);
    if (len > 0) {
        msg[len] = '\0';
        snprintf(client->nickname, sizeof(client->nickname), "%s", msg);
    }

    // 클라이언트가 보내는 메시지를 다른 클라이언트들에게 전달
    while (1) {
        len = read(client->sock, msg, sizeof(msg));
        if (len > 0) {
            msg[len] = '\0';
            printf("[%s]: %s\n", client->nickname, msg);  // 메시지 출력

            // 모든 클라이언트에게 메시지 전달 (닉네임과 함께)
            for (int i = 0; i < num_clients; i++) {
                if (clients[i].sock != client->sock) {
                    char formatted_msg[BUF_SIZE];
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s]: %s", client->nickname, msg);
                    write(clients[i].sock, formatted_msg, strlen(formatted_msg));
                }
            }
        }
    }
}

void send_to_all_clients(char *msg) {
    for (int i = 0; i < num_clients; i++) {
        write(clients[i].sock, msg, strlen(msg));
    }
}

void assign_roles() {
    int roles[MAX_USERS] = {0, 1, 2, 0, 0}; 

    for (int i = 0; i < MAX_USERS; i++) {
        int rand_idx = rand() % MAX_USERS;
        int temp = roles[i];
        roles[i] = roles[rand_idx];
        roles[rand_idx] = temp;
    }
    for (int i = 0; i < MAX_USERS; i++) {
        clients[i].role = roles[i];
        printf("Client %d assigned role: %d\n", i + 1, clients[i].role);

        // 각 클라이언트에게 자신의 역할을 전송
        char msg[BUF_SIZE];
        switch (clients[i].role) {
            case 0:
                snprintf(msg, sizeof(msg), "당신의 역할은 시민입니다.");
                break;
            case 1:
                snprintf(msg, sizeof(msg), "당신의 역할은 마피아입니다.");
                break;
            case 2:
                snprintf(msg, sizeof(msg), "당신의 역할은 경찰입니다.");
                break;
            default:
                snprintf(msg, sizeof(msg), "알 수 없는 역할입니다.");
                break;
        }
        write(clients[i].sock, msg, strlen(msg));
    }
}

void start_discussion() {
    time_t current_time;
    time(&start_time);
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "===== Day 1 - Morning =====\n[사회자] 첫 번째 아침이 왔습니다.\n[사회자] 토론을 통해 마피아를 결정해주세요!\n토론 시간은 1분입니다!\n");
    send_to_all_clients(msg);  // 모든 클라이언트에게 전송

    while (1) {
        time(&current_time);
        if (difftime(current_time, start_time) >= 60.0) {
            break;  // 1분 경과
        }
        sleep(1);
    }

    snprintf(msg, sizeof(msg), "시간이 끝났습니다! 이제 투표가 시작됩니다.\n");
    send_to_all_clients(msg);
}

void voting() {
    int vote_counts[MAX_USERS] = {0};  // 각 클라이언트에 대한 투표 수
    int voted_player;
    char msg[BUF_SIZE];

    // 각 클라이언트로부터 투표를 받음
    for (int i = 0; i < num_clients; i++) {
        snprintf(msg, sizeof(msg), "투표를 진행하세요! 누구를 추방할까요? (1~5)");
        write(clients[i].sock, msg, strlen(msg));

        read(clients[i].sock, msg, sizeof(msg));
        voted_player = atoi(msg) - 1;
        if (voted_player >= 0 && voted_player < MAX_USERS) {
            vote_counts[voted_player]++;
        }
    }

    // 가장 많은 투표를 받은 사람을 추방
    int max_votes = 0;
    int eject_index = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (vote_counts[i] > max_votes) {
            max_votes = vote_counts[i];
            eject_index = i;
        }
    }

    if (max_votes > 2) {  // 다수결로 추방
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: Player %d가 추방되었습니다!\n", eject_index + 1);
        send_to_all_clients(msg);
    } else {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
        send_to_all_clients(msg);
    }
}

void *server_handler(void *arg) {
    int server_sock = *(int *)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sock;
    pthread_t thread_id;

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            error("Accept error");
        }

        if (num_clients < MAX_USERS) {
            Client *new_client = malloc(sizeof(Client));
            new_client->sock = client_sock;
            snprintf(new_client->nickname, sizeof(new_client->nickname), "Player %d", num_clients + 1);
            clients[num_clients] = *new_client;
            num_clients++;

            printf("%s joined the game\n", new_client->nickname);

            pthread_create(&thread_id, NULL, client_handler, (void *)new_client);
            pthread_detach(thread_id);  // 메모리 누수 방지
        } else {
            char *msg = "Game is full, try again later.";
            write(client_sock, msg, strlen(msg));
            close(client_sock);
        }
    }
}

int main() {
    srand(time(NULL));

    int server_sock;
    struct sockaddr_in server_addr;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        error("Socket creation error");
    }

    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error("setsockopt error");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("Binding error");
    }

    if (listen(server_sock, 5) < 0) {
        error("Listening error");
    }

    printf("Starting server...\n");

    pthread_t server_thread;
    pthread_create(&server_thread, NULL, server_handler, (void *)&server_sock);

    // 게임 시작
    while (num_clients < MAX_USERS) {
        sleep(1);
    }

    assign_roles();  // 역할 배정
    start_discussion();  // 토론 시작
    voting();  // 투표 진행

    pthread_join(server_thread, NULL);
    close(server_sock);
    return 0;
}
