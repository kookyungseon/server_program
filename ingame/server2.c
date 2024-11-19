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
    pthread_t thread_id; 
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
        if (difftime(current_time, start_time) >= 10.0) {
            break;  // 1분 경과
        }
        sleep(1);
    }

    snprintf(msg, sizeof(msg), "시간이 끝났습니다! 이제 투표가 시작됩니다.\n");
    send_to_all_clients(msg);
}
// 투표 결과 집계 함수

// 투표 종료 및 집계 호출 함수

void exit_game(int eject_index) {
    char buffer[BUF_SIZE];

    // 추방된 클라이언트의 소켓 종료
    close(clients[eject_index].sock);
    clients[eject_index].sock = -1;  // 소켓을 -1로 설정하여 사용되지 않도록 함
    clients[eject_index].role = 0;   // 추방된 클라이언트의 역할을 일반 클라이언트로 변경
    snprintf(buffer, sizeof(buffer), "[사회자] %s님이 게임에서 추방되었습니다.\n", clients[eject_index].nickname);

    // 추방된 클라이언트의 상태를 다른 클라이언트들에게 알리기
    send_to_all_clients(buffer);

    // 추방된 클라이언트를 게임 목록에서 제거
    for (int i = eject_index; i < num_clients - 1; i++) {
        clients[i] = clients[i + 1];  // 배열에서 해당 클라이언트 제거
    }
    printf("클라이언트 %s가 게임에서 추방되고, 서버에서 종료되었습니다.\n", clients[eject_index].nickname);

    num_clients--;  // 클라이언트 수 감소

    // 추방된 클라이언트의 정보를 초기화
    memset(&clients[num_clients], 0, sizeof(Client));  // 마지막 클라이언트 정보 초기화

    // 클라이언트 스레드 종료
    if (clients[eject_index].thread_id != 0) {
        pthread_cancel(clients[eject_index].thread_id);  // 클라이언트 스레드 종료
        clients[eject_index].thread_id = 0;  // 스레드 ID 초기화
    }

    // 클라이언트와 관련된 리소스를 해제 (메모리 해제 등)
    // 예시: 필요시 동적 메모리를 할당한 경우 해당 메모리 해제


}


void start_night_mode() {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "\n=== 밤이 되었습니다. ===\n모든 플레이어가 잠에 듭니다...\n");
    send_to_all_clients(msg);

    sleep(2);

    // 1. 마피아 행동
    snprintf(msg, sizeof(msg), "[사회자] 마피아는 살해할 대상을 선택하세요. (1~%d 중 번호를 입력하세요)\n", num_clients);
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 1) { // 마피아
            write(clients[i].sock, msg, strlen(msg));
        }
    }

    int mafia_target = -1;
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 1) { // 마피아
            char input[BUF_SIZE];
            int len = read(clients[i].sock, input, sizeof(input) - 1);
            if (len > 0) {
                input[len] = '\0';
                mafia_target = atoi(input) - 1;
                if (mafia_target >= 0 && mafia_target < num_clients) {
                    break;
                }
            }
        }
    }

    // 2. 경찰 행동
    snprintf(msg, sizeof(msg), "[사회자] 경찰은 조사할 대상을 선택하세요. (1~%d 중 번호를 입력하세요)\n", num_clients);
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 2) { // 경찰
            write(clients[i].sock, msg, strlen(msg));
        }
    }

    int police_target = -1;
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 2) { // 경찰
            char input[BUF_SIZE];
            int len = read(clients[i].sock, input, sizeof(input) - 1);
            if (len > 0) {
                input[len] = '\0';
                police_target = atoi(input) - 1;
                if (police_target >= 0 && police_target < num_clients) {
                    break;
                }
            }
        }
    }

    // 3. 결과 처리
    snprintf(msg, sizeof(msg), "[사회자] 밤이 끝났습니다. 모두 일어나세요!\n");
    send_to_all_clients(msg);

    if (mafia_target != -1 && mafia_target < num_clients) {
        snprintf(msg, sizeof(msg), "[사회자] %s님이 마피아에 의해 살해되었습니다.\n", clients[mafia_target].nickname);
        send_to_all_clients(msg);
        exit_game(mafia_target); // 희생자 퇴장
    }

    if (police_target != -1 && police_target < num_clients) {
        snprintf(msg, sizeof(msg), "[사회자] 경찰은 %s님을 조사한 결과, 역할은 %s입니다.\n",
                 clients[police_target].nickname,
                 clients[police_target].role == 1 ? "마피아" : "마피아가 아님");
        for (int i = 0; i < num_clients; i++) {
            if (clients[i].role == 2) { // 경찰
                write(clients[i].sock, msg, strlen(msg));
            }
        }
    }

    snprintf(msg, sizeof(msg), "[사회자] 낮이 되었습니다. 토론을 시작하세요!\n");
    send_to_all_clients(msg);

    start_discussion(); // 낮 모드로 전환
}

void process_voting_result(int eject_index) {
    if (eject_index == -1) {
        // 아무도 추방되지 않음
        send_to_all_clients("[사회자] 아무도 추방되지 않았습니다. 밤이 시작됩니다.\n");
        start_night_mode();
 
    } else {
        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "[사회자] %s님이 가장 많은 표를 받았습니다", clients[eject_index].nickname);

        
        send_to_all_clients(msg);
        start_night_mode();

        if (clients[eject_index].role == 1) { // 마피아가 추방됨
            snprintf(msg, sizeof(msg), "[사회자] 마피아가 추방되었습니다. 게임이 종료되었습니다!\n");
            send_to_all_clients(msg);
            exit_game(eject_index); // 추방된 클라이언트 게임에서 나가게 하기
        } else {
            snprintf(msg, sizeof(msg), "[사회자] 추방된 플레이어는 마피아가 아닙니다.\n");
            send_to_all_clients(msg);
            exit_game(eject_index); // 추방된 클라이언트 게임에서 나가게 하기
         
        }
    }
}

void end_voting() {
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

    // 투표 결과 처리
    if (max_vote_count > 1) {
        // 동률인 경우 아무도 추방되지 않음
        process_voting_result(-1);
    } else {
        // 한 명이 추방됨
        process_voting_result(eject_index);
    }
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
                                write(client->sock, "투표 완료했습니다.\n", 45);
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

void display_remaining_roles(int eject_index) {
    int mafia_count = 0, police_count = 0, citizen_count = 0;
    char msg[BUF_SIZE] = "[사회자] 남은 역할별 인원수 및 역할 정보:\n";

    // num_clients 값 확인
    printf("num_clients = %d\n", num_clients);

    // Count remaining players by their roles, excluding the ejected player
    for (int i = 0; i < num_clients; i++) {
        if (i == eject_index) {
            continue; // Skip the ejected player
        }

        // Handle the remaining players' roles
        if (clients[i].role == 1) { // Mafia
            mafia_count++;
            char temp_msg[BUF_SIZE];
            snprintf(temp_msg, sizeof(temp_msg), "Player %d: 마피아\n", i + 1);
            strcat(msg, temp_msg);
        } else if (clients[i].role == 2) { // Police
            police_count++;
            char temp_msg[BUF_SIZE];
            snprintf(temp_msg, sizeof(temp_msg), "Player %d: 경찰\n", i + 1);
            strcat(msg, temp_msg);
        } else { // Citizen
            citizen_count++;
            char temp_msg[BUF_SIZE];
            snprintf(temp_msg, sizeof(temp_msg), "Player %d: 시민\n", i + 1);
            strcat(msg, temp_msg);
        }
    }

    // Add summary counts for each role
    char summary[BUF_SIZE];
    snprintf(summary, sizeof(summary), "\n마피아: %d명\n경찰: %d명\n시민: %d명\n", mafia_count, police_count, citizen_count);
    strcat(msg, summary);

    // Send the message to all clients and print to server console
    send_to_all_clients(msg);
    printf("%s", msg);
}// 역할에 따른 모드 활성화 함수



void voting() {
    int vote_counts[MAX_USERS] = {0};  // 각 클라이언트에 대한 투표 수
    char msg[BUF_SIZE];
    char voting_msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n투표를 진행하세요! 누구를 추방할까요? (1~%d 중 번호를 입력하세요\n)", MAX_USERS);
    send_to_all_clients(msg);

    // 투표 대상 클라이언트 목록 출력
    memset(voting_msg, 0, sizeof(voting_msg));  // voting_msg 초기화
    for (int i = 0; i < num_clients; i++) {
        snprintf(voting_msg + strlen(voting_msg), sizeof(voting_msg) - strlen(voting_msg),
                 "%d. %s\n", i + 1, clients[i].nickname);
    }
    strcat(voting_msg, "번호를 입력하여 투표하세요:\n");
    send_to_all_clients(voting_msg);  // 클라이언트들에게 투표 목록 전송

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
                    msg[strcspn(msg, "\n")] = '\0';

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
                        write(clients[i].sock, "잘못된 입력입니다. 다시 시도하세요\n", 45);
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
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: %s님이 선택되었습니다.\n", clients[eject_index].nickname);

    } else {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
    }

    send_to_all_clients(msg);
    printf("%s", msg);
    display_remaining_roles(eject_index);

    // 투표 종료 후 경찰 모드 활성화
    // for (int i = 0; i < num_clients; i++) {
    //     clients[i].in_police_mode = 1;  // 경찰 모드 활성화
    //     clients[i].in_voting = 0;  // 투표 모드 비활성화
    // }

    // 경찰 모드 시작 메시지
    snprintf(msg, sizeof(msg), "=== 경찰 모드가 시작되었습니다. 경찰은 조사할 대상을 선택하세요. ===\n");
    send_to_all_clients(msg);

    // 경찰에게 조사할 대상 선택을 요청하는 메시지 전송
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 2) {  // 경찰 역할을 가진 클라이언트에게만
            snprintf(msg, sizeof(msg), "경찰님, 조사를 시작하세요. (1~%d 중 번호를 입력하세요)\n", num_clients);
            write(clients[i].sock, msg, strlen(msg));
        }
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