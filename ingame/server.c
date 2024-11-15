// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <pthread.h>
// #include <time.h>

// #define MAX_USERS 5
// #define BUF_SIZE 1024
// #define PORT 8080

// typedef struct {
//     int sock;
//     char nickname[50];
//     int role;  
//     int voted_for;  // 투표한 플레이어
// } Client;

// Client clients[MAX_USERS];
// int num_clients = 0;
// time_t start_time; 
// pthread_mutex_t lock;
// pthread_t server_thread; // 서버 핸들러 스레드를 위한 식별자

// void send_to_all_clients(char *msg);

// void error(char *msg) {
//     perror(msg);
//     exit(1);
// }

// void *client_handler(void *arg) {
//     Client *client = (Client *)arg;
//     char msg[BUF_SIZE];
//     int len;

//     // 클라이언트로부터 닉네임 받기
//     len = read(client->sock, msg, sizeof(msg) - 1);
//     if (len > 0) {
//         msg[len] = '\0';
//         snprintf(client->nickname, sizeof(client->nickname), "%s", msg);
//         printf("[%s] 닉네임이 설정되었습니다.\n", client->nickname);
//     }

//     while ((len = read(client->sock, msg, sizeof(msg) - 1)) > 0) {
//         msg[len] = '\0';  // 메시지 종료 처리

//         // 투표 메시지 처리
//         if (strncmp(msg, "!vote", 5) == 0) {
//             int voted_player = atoi(&msg[6]) - 1;  // 메시지에서 투표 번호 추출
//             if (voted_player >= 0 && voted_player < MAX_USERS) {
//                 pthread_mutex_lock(&lock);
//                 client->voted_for = voted_player;  // 투표한 플레이어 저장
//                 pthread_mutex_unlock(&lock);
//                 printf("%s님이 Player %d에게 투표했습니다.\n", client->nickname, voted_player + 1);  // 서버 콘솔에 표시
//             } else {
//                 printf("%s님이 잘못된 번호에 투표했습니다: %s\n", client->nickname, msg);
//                 write(client->sock, "잘못된 투표 번호입니다. 다시 시도하세요.\n", 39);
//             }
//         } else {
//             // 채팅 메시지 처리
//             printf("[채팅] %s: %s", client->nickname, msg);
            
//             // 모든 클라이언트에게 채팅 메시지 전송 (닉네임과 메시지 포함)
//             send_to_all_clients(msg);
//         }
//     }
    
//     // 클라이언트 연결 종료 처리
//     printf("[%s] 연결 종료\n", client->nickname);
//     close(client->sock);
//     return NULL;
// }

// // 모든 클라이언트에게 메시지 전송 함수
// void send_to_all_clients(char *msg) {
//     for (int i = 0; i < num_clients; i++) {
//         write(clients[i].sock, msg, strlen(msg));
//     }
// }


// void assign_roles() {
//     int roles[MAX_USERS] = {0, 1, 2, 0, 0}; 

//     for (int i = 0; i < MAX_USERS; i++) {
//         int rand_idx = rand() % MAX_USERS;
//         int temp = roles[i];
//         roles[i] = roles[rand_idx];
//         roles[rand_idx] = temp;
//     }
//     for (int i = 0; i < MAX_USERS; i++) {
//         clients[i].role = roles[i];
//         printf("Client %d assigned role: %d\n", i + 1, clients[i].role);

//         // 각 클라이언트에게 자신의 역할을 전송
//         char msg[BUF_SIZE];
//         switch (clients[i].role) {
//             case 0:
//                 snprintf(msg, sizeof(msg), "당신의 역할은 시민입니다.");
//                 break;
//             case 1:
//                 snprintf(msg, sizeof(msg), "당신의 역할은 마피아입니다.");
//                 break;
//             case 2:
//                 snprintf(msg, sizeof(msg), "당신의 역할은 경찰입니다.");
//                 break;
//             default:
//                 snprintf(msg, sizeof(msg), "알 수 없는 역할입니다.");
//                 break;
//         }
//         write(clients[i].sock, msg, strlen(msg));
//     }
// }




// void start_discussion() {
//     time_t current_time;
//     time(&start_time);
//     char msg[BUF_SIZE];
//     snprintf(msg, sizeof(msg), "===== Day 1 - Morning =====\n[사회자] 첫 번째 아침이 왔습니다.\n[사회자] 토론을 통해 마피아를 결정해주세요!\n토론 시간은 1분입니다!\n");
//     send_to_all_clients(msg);  // 모든 클라이언트에게 전송

//     while (1) {
//         time(&current_time);
//         if (difftime(current_time, start_time) >= 60.0) {
//             break;  // 1분 경과
//         }
//         sleep(1);
//     }

//     snprintf(msg, sizeof(msg), "시간이 끝났습니다! 이제 투표가 시작됩니다.\n");
//     send_to_all_clients(msg);
// }

// void voting() {
//     int vote_counts[MAX_USERS] = {0};  // 각 클라이언트에 대한 투표 수
//     char msg[BUF_SIZE];

//     // 투표 메시지 전송
//     snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n투표를 진행하세요! 누구를 추방할까요? (1~%d 중 번호를 입력하세요)", MAX_USERS);
//     send_to_all_clients(msg);

//     // 각 클라이언트로부터 투표를 받음
//     for (int i = 0; i < num_clients; i++) {
//         // 각 클라이언트로부터 투표 입력 받음
//         read(clients[i].sock, msg, sizeof(msg));

//         // 입력값 끝에 있는 \n 제거
//         msg[strcspn(msg, "\n")] = 0;

//         // 입력값을 정수로 변환
//         int voted_player = atoi(msg) - 1;  // 1부터 시작하므로 인덱스를 위해 -1

//         // 유효한 투표 번호인지 확인 후 투표 수 증가
//         if (voted_player >= 0 && voted_player < MAX_USERS) {
//             vote_counts[voted_player]++;
//             printf("%s님이 Player %d에게 투표했습니다.\n", clients[i].nickname, voted_player + 1);  // 서버 콘솔에 표시
//         } else {
//             printf("%s님이 잘못된 번호에 투표했습니다: %s\n", clients[i].nickname, msg);
//         }
//     }

//     // 가장 많은 투표를 받은 사람을 추방
//     int max_votes = 0;
//     int eject_index = -1;
//     for (int i = 0; i < MAX_USERS; i++) {
//         if (vote_counts[i] > max_votes) {
//             max_votes = vote_counts[i];
//             eject_index = i;
//         }
//     }

//     // 결과 출력
//     if (max_votes > 0) {  // 다수결로 추방
//         snprintf(msg, sizeof(msg), "[사회자] 투표 결과: Player %d가 추방되었습니다!\n", eject_index + 1);
//         send_to_all_clients(msg);
//     } else {
//         snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
//         send_to_all_clients(msg);
//     }
// }


// void check_game_end() {
//     int mafia_count = 0;
//     int citizen_count = 0;
//     for (int i = 0; i < num_clients; i++) {
//         if (clients[i].role == 1) mafia_count++;
//         else if (clients[i].role == 0 || clients[i].role == 2) citizen_count++;
//     }

//     if (mafia_count >= citizen_count) {
//         send_to_all_clients("마피아가 다수입니다! 마피아의 승리로 게임이 종료됩니다.\n");
//         exit(0);
//     } else if (mafia_count == 0) {
//         send_to_all_clients("모든 마피아가 사살되었습니다! 시민의 승리로 게임이 종료됩니다.\n");
//         exit(0);
//     }
// }


// void *server_handler(void *arg) {
//     int server_sock = *(int *)arg;
//     struct sockaddr_in client_addr;
//     socklen_t client_len = sizeof(client_addr);
//     int client_sock;
//     pthread_t thread_id;

//     while (1) {
//         client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
//         if (client_sock < 0) {
//             error("Accept error");
//         }

//         if (num_clients < MAX_USERS) {
//             Client *new_client = malloc(sizeof(Client));
//             new_client->sock = client_sock;
//             snprintf(new_client->nickname, sizeof(new_client->nickname), "Player %d", num_clients + 1);
//             clients[num_clients] = *new_client;
//             num_clients++;

//             printf("%s joined the game\n", new_client->nickname);

//             pthread_create(&thread_id, NULL, client_handler, (void *)new_client);
//             pthread_detach(thread_id);  // 메모리 누수 방지
//         } else {
//             char *msg = "Game is full, try again later.";
//             write(client_sock, msg, strlen(msg));
//             close(client_sock);
//         }
//     }
// }

// int main() {
//     srand(time(NULL));
//     int server_sock;
//     struct sockaddr_in server_addr;

//     // 서버 소켓 생성
//     server_sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_sock < 0) {
//         error("socket failed");
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_addr.s_addr = INADDR_ANY;
//     server_addr.sin_port = htons(PORT);

//     if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         error("bind failed");
//     }

//     if (listen(server_sock, 5) < 0) {
//         error("listen failed");
//     }

//     printf("서버가 시작되었습니다. 연결을 기다립니다...\n");

//     // 게임을 위한 서버 핸들러 스레드 시작
//     pthread_create(&server_thread, NULL, server_handler, (void *)&server_sock);

//     // 게임 로직 실행
//     assign_roles();
//     start_discussion();
//     voting();  // 투표 진행
//     check_game_end();

//     return 0;
// }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h> 
 

#define MAX_USERS 5
#define BUF_SIZE 1024
#define PORT 8080

typedef struct {
    int sock;
    char nickname[50];
    int role;
    int voted_for;
    int in_voting;  // 투표 중인지 여부
} Client;

Client clients[MAX_USERS];
int num_clients = 0;
time_t start_time;
pthread_mutex_t lock;
int vote_counts[MAX_USERS] = {0};
int votes_received = 0;  // 각 클라이언트에 대한 투표 수

void error(char *msg) {
    perror(msg);
    exit(1);
}
// void *client_handler(void *arg) {
//     Client *client = (Client *)arg;
//     char msg[BUF_SIZE];
//     int len;

//     // 클라이언트로부터 닉네임 받기
//     len = read(client->sock, msg, sizeof(msg) - 1);
//     if (len > 0) {
//         msg[len] = '\0';
//         snprintf(client->nickname, sizeof(client->nickname), "%s", msg);
//     }

//     // 클라이언트가 보내는 메시지를 다른 클라이언트들에게 전달
//     while (1) {
//         len = read(client->sock, msg, sizeof(msg));
//         if (len > 0) {
//             msg[len] = '\0';

//             // 채팅 모드와 투표 모드를 구분
//             if (client->in_voting) {
//                 // 투표 중일 때만 입력받고, 채팅은 금지
//                 if (msg[0] >= '1' && msg[0] <= '5' && msg[1] == '\0') {
//                     // 1~5 사이의 숫자만 유효한 투표 번호
//                     int voted_player = msg[0] - '1';  // 문자로부터 숫자 추출
//                     if (voted_player >= 0 && voted_player < MAX_USERS) {
//                         pthread_mutex_lock(&lock);
//                         client->voted_for = voted_player;  // 투표한 플레이어 저장
//                         pthread_mutex_unlock(&lock);
//                         printf("%s님이 Player %d에게 투표했습니다.\n", client->nickname, voted_player + 1);
//                     } else {
//                         printf("%s님이 잘못된 번호에 투표했습니다: %s\n", client->nickname, msg);
//                         write(client->sock, "잘못된 투표 번호입니다. 다시 시도하세요.\n", 39);
//                     }
//                 } else {
//                     write(client->sock, "잘못된 입력입니다.\n", 45);
//                 }
//             } else {
//                 // 채팅 중일 때
//                 printf("[채팅] %s: %s", client->nickname, msg);
//                 // 모든 클라이언트에게 채팅 메시지 전송 (닉네임과 메시지 포함)
//                 for (int i = 0; i < num_clients; i++) {
//                     if (clients[i].sock != client->sock) {
//                         char formatted_msg[BUF_SIZE];
//                         snprintf(formatted_msg, sizeof(formatted_msg), "[%s]: %s", client->nickname, msg);
//                         write(clients[i].sock, formatted_msg, strlen(formatted_msg));
//                     }
//                 }
//             }
//         }
//     }
// }



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
// 투표 결과 집계 함수
void tally_votes_and_display_results() {
    int max_votes = 0;
    int eject_index = -1;
    int max_vote_count = 0;

    // 투표 결과 집계
    for (int i = 0; i < num_clients; i++) {
        if (vote_counts[i] > max_votes) {
            max_votes = vote_counts[i];
            eject_index = i;
            max_vote_count = 1;
        } else if (vote_counts[i] == max_votes && max_votes > 0) {
            max_vote_count++;
        }
    }

    // 결과 메시지 생성
    char result_msg[BUF_SIZE];
    if (max_vote_count > 1) {
        snprintf(result_msg, sizeof(result_msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
    } else if (max_votes > 0) {
        snprintf(result_msg, sizeof(result_msg), "[사회자] 투표 결과: Player %d가 추방되었습니다!\n", eject_index + 1);
    } else {
        snprintf(result_msg, sizeof(result_msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
    }

    // 모든 클라이언트에게 결과 전송
    send_to_all_clients(result_msg);
    printf("%s", result_msg);
}

// 투표 종료 및 집계 호출 함수
void end_voting() {
    for (int i = 0; i < num_clients; i++) {
        clients[i].in_voting = 0;  // 투표 모드 종료
    }
    tally_votes_and_display_results();  // 투표 결과 집계 함수 호출
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

    // 클라이언트가 보내는 메시지를 처리
    while (1) {
        len = read(client->sock, msg, sizeof(msg));
        if (len > 0) {
            msg[len] = '\0';
            msg[strcspn(msg, "\n")] = '\0';

            // 투표 모드와 채팅 모드 구분
            if (client->in_voting) {
                // 투표 중일 때만 입력받고, 채팅은 금지
                if (msg[0] >= '1' && msg[0] <= '5' && msg[1] == '\0') {
                    // 1~5 사이의 숫자만 유효한 투표 번호
                    int voted_player = msg[0] - '1';  // 문자로부터 숫자 추출
                    if (voted_player >= 0 && voted_player < num_clients) {
                        pthread_mutex_lock(&lock);
                        if (client->voted_for == -1) {  // 투표한 적이 없는 경우에만 투표 반영
                            vote_counts[voted_player]++;
                            client->voted_for = voted_player;  // 투표한 플레이어 저장
                            votes_received++;
                            printf("%s님이 Player %d에게 투표했습니다.\n", client->nickname, voted_player + 1);

                            // 모든 클라이언트가 투표를 완료했으면 투표 종료 및 결과 집계 호출
                            if (votes_received == num_clients) {
                                end_voting();
                            }
                        } else {
                            write(client->sock, "이미 투표를 완료했습니다.\n", 45);
                        }
                        pthread_mutex_unlock(&lock);
                    } else {
                        write(client->sock, "잘못된 투표 번호입니다. 다시 시도하세요.\n", 39);
                    }
                } else {
                    printf("입력값: '%s'\n", msg);  // 디버깅을 위한 출력
                    write(client->sock, "잘못된 입력입니다.\n", 45);
                }
            } else {
                // 채팅 중일 때
                printf("[채팅] %s: %s\n", client->nickname, msg);
                // 모든 클라이언트에게 채팅 메시지 전송 (닉네임과 메시지 포함)
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
}
// 투표 종료 후 결과 집계 함수
void voting() {
    int vote_counts[MAX_USERS] = {0};  // 각 클라이언트에 대한 투표 수
    char msg[BUF_SIZE];
    
    snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n투표를 진행하세요! 누구를 추방할까요? (1~%d 중 번호를 입력하세요)", MAX_USERS);
    send_to_all_clients(msg);

    // 투표 모드로 전환
    for (int i = 0; i < num_clients; i++) {
        clients[i].in_voting = 1;  // 투표 모드 활성화
        clients[i].voted_for = -1;  // 초기화
    }

    int votes_received = 0;
    while (votes_received < num_clients) {
        for (int i = 0; i < num_clients; i++) {
            if (clients[i].in_voting) {
                int len = read(clients[i].sock, msg, sizeof(msg) - 1);
                if (len > 0) {
                    msg[len] = '\0';

                    // 입력값에서 개행 문자 제거
                    msg[strcspn(msg, "\n")] = '\0';

                    // 입력값을 서버 콘솔에 출력 (디버깅용)
                    printf("Received input from %s: '%s'\n", clients[i].nickname, msg);

                    // 입력값이 숫자인지 확인하고 유효 범위인지 검사
                    if (isdigit(msg[0]) && msg[1] == '\0') {
                        int voted_player = atoi(msg) - 1;  // 문자열을 정수로 변환
                        if (voted_player >= 0 && voted_player < MAX_USERS) {
                            pthread_mutex_lock(&lock);
                            if (clients[i].voted_for == -1) {  // 중복 투표 방지
                                clients[i].voted_for = voted_player;
                                vote_counts[voted_player]++;
                                votes_received++;
                                printf("%s님이 Player %d에게 투표했습니다.\n", clients[i].nickname, voted_player + 1);
                            }
                            clients[i].in_voting = 0;  // 투표 완료
                            pthread_mutex_unlock(&lock);
                        } else {
                            write(clients[i].sock, "잘못된 투표 번호입니다. 다시 시도하세요.\n", 39);
                        }
                    } else {
                        write(clients[i].sock, "잘못된 입력입니다. 숫자 1~5를 입력하세요.\n", 45);
                    }
                }
            }
        }
    }

    // 투표 종료 후 처리
    int max_votes = 0;
    int eject_index = -1;
    int max_vote_count = 0;

    // 최다 득표 수와 동점 여부 확인
    for (int i = 0; i < MAX_USERS; i++) {
        if (vote_counts[i] > max_votes) {
            max_votes = vote_counts[i];
            eject_index = i;
            max_vote_count = 1;
        } else if (vote_counts[i] == max_votes && max_votes > 0) {
            max_vote_count++;
        }
    }

    // 동점 확인 후 결과 메시지 생성
    if (max_vote_count > 1) {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
    } else if (max_votes > 0) {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: Player %d가 추방되었습니다!\n", eject_index + 1);
    } else {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
    }

    send_to_all_clients(msg);
    printf("%s", msg);
}


// void voting() {
//     int vote_counts[MAX_USERS] = {0};  // 각 클라이언트에 대한 투표 수
//     char msg[BUF_SIZE];

//     // 플레이어 목록과 투표 시작 메시지 전송
//     snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n");
//     send_to_all_clients(msg);
    
//     for (int i = 0; i < num_clients; i++) {
//         snprintf(msg, sizeof(msg), "%d. %s\n", i + 1, clients[i].nickname);
//         send_to_all_clients(msg);
//     }
//     send_to_all_clients("누구를 추방할까요? (번호를 입력하세요)");

//     // 투표 모드로 전환
//     for (int i = 0; i < num_clients; i++) {
//         clients[i].in_voting = 1;
//     }

//     // 투표를 비동기적으로 수집
//     int votes_received = 0;
//     while (votes_received < num_clients) {
//         for (int i = 0; i < num_clients; i++) {
//             if (clients[i].in_voting && read(clients[i].sock, msg, sizeof(msg)) > 0) {
//                 msg[strcspn(msg, "\n")] = 0;

//                 // 유효한 투표인지 확인
//                 int voted_player = atoi(msg) - 1;
//                 if (voted_player >= 0 && voted_player < num_clients) {
//                     vote_counts[voted_player]++;
//                     printf("%s님이 Player %d에게 투표했습니다.\n", clients[i].nickname, voted_player + 1);
//                     clients[i].in_voting = 0;  // 투표 완료
//                     votes_received++;
//                 } else {
//                     write(clients[i].sock, "잘못된 투표 번호입니다. 다시 시도하세요.\n", 39);
//                 }
//             }
//         }
//     }

//     // 투표 모드 종료
//     for (int i = 0; i < num_clients; i++) {
//         clients[i].in_voting = 0;
//     }

//     // 가장 많은 투표를 받은 사람을 추방
//     int max_votes = 0;
//     int eject_index = -1;
//     for (int i = 0; i < num_clients; i++) {
//         if (vote_counts[i] > max_votes) {
//             max_votes = vote_counts[i];
//             eject_index = i;
//         }
//     }

//     // 결과 출력
//     if (max_votes > 0) {
//         snprintf(msg, sizeof(msg), "[사회자] 투표 결과: Player %d (%s)가 추방되었습니다!\n", eject_index + 1, clients[eject_index].nickname);
//         send_to_all_clients(msg);
//         printf("[사회자] 투표 결과: Player %d (%s)가 추방되었습니다!\n", eject_index + 1, clients[eject_index].nickname);
//     } else {
//         send_to_all_clients("[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
//         printf("[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
//     }
// }


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
            clients[num_clients].sock = client_sock;
            num_clients++;

            pthread_create(&thread_id, NULL, client_handler, (void *)&clients[num_clients - 1]);
        } else {
            write(client_sock, "서버가 꽉 찼습니다. 나중에 다시 시도해 주세요.\n", 44);
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