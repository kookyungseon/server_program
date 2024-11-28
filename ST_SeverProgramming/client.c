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

int tcp_connect(int af, char *servip, unsigned short port);
void error(char* msg){perror(msg); exit(1);}

int main(int argc, char* argv[]){
    int s;
	char msg[1024];
    int max_socket_num;
    int length;
    fd_set fds;
    s = tcp_connect(AF_INET, argv[1], atoi(argv[2]));
    if(s==-1){error("socket error");}
    
    max_socket_num = s+1; // select의 범위를 정해주기 위해 설정 
    
    printf("Joined Sever!\n");
	while(1){
        FD_ZERO(&fds); // fds를 모두 0으로 세
        FD_SET(0,&fds); // 표준입력을 SET
        FD_SET(s,&fds); // 소켓 SET
        if(select(max_socket_num, &fds, NULL, NULL, NULL) < 0) error("select fail"); // select로 어떤 FD_NUM에 신호(변화)가 생겼는지 확인, select에서 기다리는 역할을 하는듯.
        // 이후 FD_ISSET으로 처리
        // 별도의 if문으로 처리? else if로 묶기?
        if(FD_ISSET(0,&fds)){
            fgets(msg,sizeof(msg),stdin);
            msg[strlen(msg)-1] = '\0';
            write(s,msg,strlen(msg));
        }
        if(FD_ISSET(s,&fds)){
            length = read(s,msg,sizeof(msg));
            msg[length] = 0;
            printf("%s\n",msg);
            if(strcmp(msg,"EXIT SERVER\n")==0) exit(0);
        }
        length = 0;
        strcpy(msg,"\0");
        //write(s,msg,strlen(msg)); // s(fd_num)은 sever와 연결되어 있음, s를 통해 데이터를 주기도 받기도 함.
	}
}

int tcp_connect(int af, char *servip, unsigned short port) {
	struct sockaddr_in servaddr;
	int  s;
    char buf[256];
	// 소켓 생성
	if ((s = socket(af, SOCK_STREAM, 0)) < 0)
		return -1;
	// 채팅 서버의 소켓주소 구조체 servaddr 초기화
	bzero((char *)&servaddr, sizeof(servaddr));
	servaddr.sin_family = af;
	inet_pton(AF_INET, servip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	// 연결요청
	if (connect(s, (struct sockaddr *)&servaddr, sizeof(servaddr))
		< 0)
		return -1;
        
	return s;
}
