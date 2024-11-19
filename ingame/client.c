#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 1024

int tcp_connect(int af, char *servip, unsigned short port);
void error(char* msg) {
    perror(msg); 
    exit(1);
}

void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    int s;
    char msg[BUF_SIZE];
    char nickname[50];
    int max_socket_num;
    int length;
    fd_set fds;
    
    // 서버 연결
    s = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
    if(s == -1) {
        error("socket error");
    }
    
    // 닉네임 입력
    printf("닉네임을 입력하세요: ");
    fgets(nickname, sizeof(nickname), stdin);
    nickname[strcspn(nickname, "\n")] = 0;
    write(s, nickname, strlen(nickname));
    
    max_socket_num = s + 1;
    printf("서버에 접속했습니다!\n");
    
    while(1) {
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(s, &fds);
        
        if(select(max_socket_num, &fds, NULL, NULL, NULL) < 0) {
            error("select fail");
        }
        
        // 서버로부터 메시지 수신
        if(FD_ISSET(s, &fds)) {
            length = read(s, msg, sizeof(msg) - 1);
            if(length <= 0) {
                printf("서버와의 연결이 끊어졌습니다.\n");
                break;
            }
            msg[length] = 0;
            printf("%s", msg);  // 서버 메시지 출력
        }
        
        // 사용자 입력 처리
        if(FD_ISSET(0, &fds)) {
            fgets(msg, sizeof(msg), stdin);
            if(strlen(msg) > 0) {
                write(s, msg, strlen(msg));
            }
        }
        
        length = 0;
        memset(msg, 0, sizeof(msg));
    }
    
    close(s);
    return 0;
}

int tcp_connect(int af, char *servip, unsigned short port) {
    struct sockaddr_in servaddr;
    int s;
    
    // 소켓 생성
    if ((s = socket(af, SOCK_STREAM, 0)) < 0)
        return -1;
    
    // 서버 주소 구조체 초기화
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = af;
    inet_pton(AF_INET, servip, &servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    
    // 연결 요청
    if (connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        return -1;
    
    return s;
}