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
    int role;           // 0: 시민, 1: 마피아, 2: 경찰
    int voted_for;
    int in_voting;      // 투표 중인지 여부
    int is_dead;        // 사망 여부
    int in_night_mode;  // 밤 모드 여부 추가
} Client;
typedef struct {
    int is_dead;         // 플레이어가 죽었는지 여부
    int investigated;    // 경찰에게 조사받았는지 여부
} PlayerStatus;

Client clients[MAX_USERS];
PlayerStatus player_status[MAX_USERS];
int num_clients = 0;
time_t start_time;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int vote_counts[MAX_USERS] = {0};
int votes_received = 0;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void send_to_all_clients(char *msg) {
    for (int i = 0; i < num_clients; i++) {
        if (!clients[i].is_dead) {  // 살아있는 클라이언트에게만 전송
            write(clients[i].sock, msg, strlen(msg));
        }
    }
}

void assign_roles() {
    int roles[MAX_USERS] = {0, 1, 2, 0, 0}; // 1명의 마피아, 1명의 경찰, 3명의 시민

    // 역할을 무작위로 섞기
    for (int i = 0; i < MAX_USERS; i++) {
        int rand_idx = rand() % MAX_USERS;
        int temp = roles[i];
        roles[i] = roles[rand_idx];
        roles[rand_idx] = temp;
    }

    // 각 클라이언트에게 역할 할당
    for (int i = 0; i < MAX_USERS; i++) {
        clients[i].role = roles[i];
        char msg[BUF_SIZE];
        
        switch (roles[i]) {
            case 0:
                snprintf(msg, sizeof(msg), "당신의 역할은 시민입니다.\n");
                break;
            case 1:
                snprintf(msg, sizeof(msg), "당신의 역할은 마피아입니다.\n");
                break;
            case 2:
                snprintf(msg, sizeof(msg), "당신의 역할은 경찰입니다.\n");
                break;
        }
        write(clients[i].sock, msg, strlen(msg));
    }
}

void start_discussion() {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "\n===== Discussion Time =====\n[사회자] 토론 시간입니다. (60초)\n");
    send_to_all_clients(msg);

    // 60초 토론 시간
    sleep(10);

    snprintf(msg, sizeof(msg), "\n[사회자] 토론 시간이 종료되었습니다. 투표를 시작합니다.\n");
    send_to_all_clients(msg);
}
void process_voting_result(int eject_index) {
    char msg[BUF_SIZE];
    
    if (eject_index == -1) {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
        send_to_all_clients(msg);
    } else {
        clients[eject_index].is_dead = 1;
        player_status[eject_index].is_dead = 1;
        
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: %s님이 추방되었습니다.\n", 
                clients[eject_index].nickname);
        send_to_all_clients(msg);
        
        // 추방된 사람에게도 결과 메시지 전송
        snprintf(msg, sizeof(msg), "[사회자] 당신은 추방되었습니다.\n");
        write(clients[eject_index].sock, msg, strlen(msg));

        if (clients[eject_index].role == 1) {
            snprintf(msg, sizeof(msg), "[사회자] 추방된 플레이어는 마피아였습니다!\n");
        } else {
            snprintf(msg, sizeof(msg), "[사회자] 추방된 플레이어는 마피아가 아니었습니다.\n");
        }
        send_to_all_clients(msg);
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
 int get_alive_players_count() {
    int count = 0;
    for (int i = 0; i < num_clients; i++) {
        if (!player_status[i].is_dead) {
            count++;
        }
    }
    return count;
}

void voting() {
    printf("[DEBUG] 투표 시작\n");
    
    // 투표 변수 초기화
    memset(vote_counts, 0, sizeof(vote_counts));
    votes_received = 0;
    
    char msg[BUF_SIZE];
    char voting_msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n투표를 진행하세요! 누구를 추방할까요? (1~%d 중 번호를 입력하세요)\n", num_clients);
    send_to_all_clients(msg);

    // 투표 대상 목록 출력
    memset(voting_msg, 0, sizeof(voting_msg));
    for (int i = 0; i < num_clients; i++) {
        if (!player_status[i].is_dead) {  // 살아있는 플레이어만 표시
            snprintf(voting_msg + strlen(voting_msg), sizeof(voting_msg) - strlen(voting_msg),
                     "%d. %s\n", i + 1, clients[i].nickname);
        }
    }
    send_to_all_clients(voting_msg);
    printf("[DEBUG] 투표 목록 전송 완료\n");

    // 투표 모드 활성화
    for (int i = 0; i < num_clients; i++) {
        if (!player_status[i].is_dead) {
            clients[i].in_voting = 1;
            clients[i].voted_for = -1;
        }
    }

    // 투표 처리
    while (votes_received < get_alive_players_count()) {
        printf("[DEBUG] 현재 투표 수: %d, 필요 투표 수: %d\n", votes_received, get_alive_players_count());
        sleep(2);  // CPU 사용률 감소를 위한 짧은 대기
    }

    printf("[DEBUG] 투표 종료, 결과 처리 시작\n");
    end_voting();
}void night_mode() {
    char msg[BUF_SIZE];
    int mafia_choice = -1;

    // 밤이 시작됨을 알림
    snprintf(msg, sizeof(msg), "\n===== Night Time =====\n[사회자] 밤이 되었습니다.\n");
    send_to_all_clients(msg);
    
    // 경찰 차례 알림
    snprintf(msg, sizeof(msg), "[사회자] 경찰이 조사를 시작합니다.\n");
    send_to_all_clients(msg);

    // 경찰 턴 처리
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 2 && !clients[i].is_dead) {
            char player_list[BUF_SIZE] = "=== 조사할 플레이어 선택 ===\n";
            for (int j = 0; j < num_clients; j++) {
                if (!clients[j].is_dead && i != j) {
                    snprintf(player_list + strlen(player_list), 
                            BUF_SIZE - strlen(player_list),
                            "%d. %s\n", j + 1, clients[j].nickname);
                }
            }
            strcat(player_list, "번호를 입력하세요: ");
            write(clients[i].sock, player_list, strlen(player_list));
            
            // 경찰을 밤 모드로 설정
            clients[i].in_night_mode = 1;
            clients[i].voted_for = -1;

            // 경찰의 선택을 기다림
            while (clients[i].in_night_mode) {
                usleep(100000);  // 0.1초 대기
            }

            int police_choice = clients[i].voted_for;
            if (police_choice >= 0 && police_choice < num_clients) {
                char result[BUF_SIZE];
                const char* role_str;
                
                // 역할을 문자열로 변환
                switch(clients[police_choice].role) {
                    case 0:
                        role_str = "시민";
                        break;
                    case 1:
                        role_str = "마피아";
                        break;
                    case 2:
                        role_str = "경찰";
                        break;
                    default:
                        role_str = "알 수 없음";
                        break;
                }

                if (clients[police_choice].role == 1) {
                    snprintf(result, sizeof(result), 
                            "[조사 결과] %s님은 마피아입니다.\n"
                            "조사 대상의 역할: %s\n", 
                            clients[police_choice].nickname, role_str);
                } else {
                    snprintf(result, sizeof(result), 
                            "[조사 결과] %s님은 마피아가 아닙니다.\n"
                            "조사 대상의 역할: %s\n", 
                            clients[police_choice].nickname, role_str);
                }
                write(clients[i].sock, result, strlen(result));

                // 경찰의 조사가 끝났음을 알림
                snprintf(msg, sizeof(msg), "[사회자] 경찰이 조사를 마쳤습니다.\n");
                send_to_all_clients(msg);
            }
        }
    }

    // 마피아 차례 알림
    snprintf(msg, sizeof(msg), "[사회자] 마피아가 희생자를 선택합니다.\n");
    send_to_all_clients(msg);

    // 마피아 턴 처리
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].role == 1 && !clients[i].is_dead) {
            char player_list[BUF_SIZE] = "=== 희생자 선택 ===\n선택 가능한 플레이어:\n";
            for (int j = 0; j < num_clients; j++) {
                if (!clients[j].is_dead && i != j) {
                    snprintf(player_list + strlen(player_list), 
                            BUF_SIZE - strlen(player_list),
                            "%d. %s\n", j + 1, clients[j].nickname);
                }
            }
            strcat(player_list, "번호를 입력하세요: ");
            write(clients[i].sock, player_list, strlen(player_list));
            
            // 마피아를 밤 모드로 설정
            clients[i].in_night_mode = 1;
            clients[i].voted_for = -1;

            // 마피아의 선택을 기다림
            while (clients[i].in_night_mode) {
                usleep(100000);  // 0.1초 대기
            }

            mafia_choice = clients[i].voted_for;
            if (mafia_choice >= 0 && mafia_choice < num_clients) {
                // 마피아가 선택을 완료했음을 알림
                snprintf(msg, sizeof(msg), "[사회자] 마피아가 선택을 마쳤습니다.\n");
                send_to_all_clients(msg);
                break;
            }
        }
    }

    // 밤이 끝나감을 알림
    snprintf(msg, sizeof(msg), "[사회자] 밤이 끝나갑니다...\n");
    sleep(2);  // 극적인 효과를 위한 짧은 대기
    send_to_all_clients(msg);

    // 밤 결과 처리
    if (mafia_choice != -1) {
        clients[mafia_choice].is_dead = 1;
        player_status[mafia_choice].is_dead = 1;
        snprintf(msg, sizeof(msg), 
                "\n===== Morning =====\n[사회자] %s님이 밤사이 사망하셨습니다.\n", 
                clients[mafia_choice].nickname);
        send_to_all_clients(msg);
    } else {
        send_to_all_clients("\n===== Morning =====\n[사회자] 아무도 사망하지 않았습니다.\n");
    }
}
int check_game_end() {
    int mafia_count = 0;
    int citizen_count = 0;
    
    for (int i = 0; i < num_clients; i++) {
        if (!clients[i].is_dead) {
            if (clients[i].role == 1) {
                mafia_count++;
            } else {
                citizen_count++;
            }
        }
    }
    
    if (mafia_count == 0) {
        // 시민 승리 메시지, 모든 클라이언트에게 전송
        for (int i = 0; i < num_clients; i++) {
            if (!clients[i].is_dead) {
                write(clients[i].sock, "\n[사회자] 시민 팀이 승리했습니다!\n", strlen("\n[사회자] 시민 팀이 승리했습니다!\n"));
            } else {
                // 사망한 플레이어에게도 결과 전송
                write(clients[i].sock, "\n[사회자] 시민 팀이 승리했습니다! (게임 종료)\n", strlen("\n[사회자] 시민 팀이 승리했습니다! (게임 종료)\n"));
            }
        }
        return 1;
    } else if (mafia_count >= citizen_count) {
        // 마피아 승리 메시지, 모든 클라이언트에게 전송
        for (int i = 0; i < num_clients; i++) {
            if (!clients[i].is_dead) {
                write(clients[i].sock, "\n[사회자] 마피아가 승리했습니다!\n", strlen("\n[사회자] 마피아가 승리했습니다!\n"));
            } else {
                // 사망한 플레이어에게도 결과 전송
                write(clients[i].sock, "\n[사회자] 마피아가 승리했습니다! (게임 종료)\n", strlen("\n[사회자] 마피아가 승리했습니다! (게임 종료)\n"));
            }
        }
        return 1;
    }
    
    return 0;
}
void *client_handler(void *arg) {
    Client *client = (Client *)arg;
    char msg[BUF_SIZE];
    int len;

    // 닉네임 받기
    len = read(client->sock, msg, sizeof(msg) - 1);
    if (len > 0) {
        msg[len] = '\0';
        msg[strcspn(msg, "\n")] = 0;
        strncpy(client->nickname, msg, sizeof(client->nickname) - 1);
        
        char welcome_msg[BUF_SIZE];
        snprintf(welcome_msg, sizeof(welcome_msg), "[알림] %s님이 입장하셨습니다.\n", client->nickname);
        send_to_all_clients(welcome_msg);
    }

    while (1) {
        len = read(client->sock, msg, sizeof(msg) - 1);
        if (len <= 0) {
            char disconnect_msg[BUF_SIZE];
            snprintf(disconnect_msg, sizeof(disconnect_msg), "[알림] %s님이 퇴장하셨습니다.\n", client->nickname);
            send_to_all_clients(disconnect_msg);
            break;
        }
        
        msg[len] = '\0';
        msg[strcspn(msg, "\n")] = 0;

        if (client->in_night_mode) {
            // 밤 모드 투표 처리
            int choice = atoi(msg) - 1;
            if (choice >= 0 && choice < num_clients) {
                // 선택 완료 메시지를 클라이언트에게 보냄
                write(client->sock, "선택이 완료되었습니다.\n", strlen("선택이 완료되었습니다.\n"));
                client->voted_for = choice;
                client->in_night_mode = 0;  // 투표 완료 후 밤 모드 해제
            }
        }
        else if (client->in_voting) {
            // 일반 투표 처리
            int vote = atoi(msg) - 1;
            if (vote >= 0 && vote < num_clients) {
                pthread_mutex_lock(&lock);
                if (client->voted_for == -1) {
                    vote_counts[vote]++;
                    client->voted_for = vote;
                    votes_received++;
                    
                    char vote_msg[BUF_SIZE];
                    snprintf(vote_msg, sizeof(vote_msg), "[알림] %s님이 투표하셨습니다.\n", client->nickname);
                    send_to_all_clients(vote_msg);
                }
                pthread_mutex_unlock(&lock);
            }
        } else {
            // 일반 채팅 메시지 처리
            if (!client->is_dead) {
                char chat_msg[BUF_SIZE];
                snprintf(chat_msg, sizeof(chat_msg), "[%s]: %s\n", client->nickname, msg);
                send_to_all_clients(chat_msg);
            }
        }
    }

    close(client->sock);
    return NULL;
}
void game_loop() {
    // 게임 초기화
    memset(player_status, 0, sizeof(player_status));
    
    // 역할 배정
    assign_roles();
    
    while (1) {
        start_discussion();    // 낮 토론
        if (check_game_end()) break;
        
        voting();             // 투표
        if (check_game_end()) break;
        
        night_mode();         // 밤 모드
        if (check_game_end()) break;
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


    game_loop(); // 역할 배정

    pthread_join(server_thread, NULL);
    close(server_sock);
    return 0;
}