// 팀 : 17팀
// 멤버 : 김태영, 구경선
// 주제 : 소켓 프로그래밍을 활용한 '마피아 게임' 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>


#define MAX_LENGTH 512 
#define MAX_SOCKET 1024
#define BUF_SIZE 1024
#define MAX_USERS 5

// 페이지 관리 함수 모음 
int tcp_listen(int host, int port, int backlog);
int max_socket_num();
void error(char *msg){perror(msg); exit(1);}
void add_player(int socket_num, struct sockaddr_in *client_addr);
void _main();
void chatting();
void* selecter(void* socket_num);
void page1(int n,int s_n);
void page1_1(int socket_num);
int login_id(int socket_num);
int login_pw(int socket_num,char* s);
void page1_4(int socket_num);
void page1_2(int s_n);
void page1_3(int s_n);
void page1_0(int s_n);
int check_dupli_id(char* input);
int check_dupli_un(char* input);
void page2(int n,int s_n);
void page2_0(int s_n);
void page2_n(int s_n,int n);
void make_room(int s_n,int r_n);
void page2_r(int s_n);
void reset_room();
void page3(int n, int s_n);
void page3_1(int s_n);
void page3_0(int s_n);
void change_room_list(int r_n);

// 인게임 관련 함수 모음 
void send_to_all_clients(int r_n,char *msg);
void assign_roles(int r_n);
void start_discussion(int r_n);
void process_voting_result(int r_n, int eject_index);
void end_voting(int r_n);
int get_alive_players_count(int r_n);
void voting(int r_n);
void night_mode(int r_n);
int check_game_end(int r_n);
void *client_handler(void *arg); // 제거 체크
void game_loop(pthread_t *thread_id,int r_n);


// 접속한 사용자의 정보를 담는 구조체 
// 사용자의 page마다 다른 함수를 적용할 것이므로 page 변수 필요 
struct users{
    char name[20];
    int page; 
    int room;
    int in_game;
};

// 생성된 방에 대한 정보
// 방제목, 인원, 들어온 유저 닉네임, 들어온 유저 소켓, 방 삭제 여부
struct rooms{
    char name[100]; 
    int head;
    char u_n[5][100]; 
    int u_s[5]; 
    int del;
};


// in-game에서 사용할 유저 정보 
// 소켓번호, 스레드, 닉네임 .... 
struct Clients{
    int sock;
    pthread_t thread_id;
    char nickname[50];
    int role;           // 0: 시민, 1: 마피아, 2: 경찰
    int voted_for;
    int in_voting;      // 투표 중인지 여부
    int is_dead;        // 사망 여부
    int in_night_mode;  // 밤 모드 여부 추가
    int r_n; // 게임이 진행되는 방 넘버 
};

// in-game에서 사용할 유저 상태 정보 
struct PlayerStatus{
    int is_dead;         // 플레이어가 죽었는지 여부
    int investigated;
};

// 게임이 진행되는 방에서 사용할 변수 
struct game_rooms{
    struct Clients clients[5];
    struct PlayerStatus player_status[5];
    int num_clients;
    time_t start_time;
    pthread_mutex_t lock;
    int vote_counts[5];
    int votes_received;
    int game_status;
};

// 함수의 편리를 위해 전역변수 선언 
int player[MAX_SOCKET]; // 플레이어 소켓 
int player_num = 0; // 플레이어 숫자
int listen_socket; // listen에 사용할 소켓 
struct users user[MAX_SOCKET]; 
int sign[BUF_SIZE] ={0,}; // thread가 중첩되지 않도록 thread create를 사용하고 있는 스레드는 sign bit을 1로 만들어 더이상 입력을 받지 못하게 처리 
int room_num = 1; // 할당할 방 번호 
struct rooms rooms[MAX_SOCKET]; // 방 정보를 간략하게 저장하는 구조체
struct game_rooms game_rooms[MAX_SOCKET];
int close_socket[MAX_SOCKET] = {0,}; // 유저가 게임에서 나갈 때 처리과정이 엉켜서 오류가 생김. 이를 막기 위해 나간 유저를 close_socket에 1로 등록하여 이 유저의 입력은 받지 않도록 처리

fd_set fds;


int main(int argc, char* argv[]){
    int length;
    struct sockaddr_in addr;
    pthread_t thread[MAX_SOCKET];
    char msg[MAX_LENGTH];
    int player_socket_num; // 플레이어 소켓 임시 저장 변수 
    int struct_len = sizeof(struct sockaddr_in);
    int socket_max;
    reset_room();    

    if (argc != 2){printf("Please insert 'Port' in argument\n"); exit(0);}


    listen_socket = tcp_listen(INADDR_ANY, atoi(argv[1]), 5); // listen 소켓 생성 
    socket_max = listen_socket;
    printf("Hello Server!\n");
    while(1){
        socket_max = 0;
        FD_ZERO(&fds); // 모든 FD_NUM에 대하여 0으로 세팅 
        FD_SET(listen_socket, &fds); // FD_SET은 select에서 '검사할' FD_NUM을 세팅 
        for (int i = 0; i < player_num; i++){
            if(close_socket[player[i]]==0 && user[player[i]].in_game==0) FD_SET(player[i], &fds);
            } // 현재 존재하는 모든 플레이어들에 대하여 FD_NUM 세팅, 접속을 종료한 플레이어는 FD_NUM 세팅 X  

        socket_max += max_socket_num(); // select에서 검사 범위를 지정하기 위해 갖고 있는 소켓 중 가장 큰 FD_NUM 확인
        if(select(socket_max, &fds, NULL, NULL, NULL) < 0){error("select error");} // fd의 변화를 감지하는 함수, 이를 통해 클라이언트의 접속을 감지할 수 있음. 
        

        if(FD_ISSET(listen_socket, &fds)){ // 접속을 담당하는 소켓에 1이 세팅 => 어떤 사용자가 서버에 접속함
        // 사용자의 접속이 없었다면 위 if문에서 listen_socket을 검사했을 때 FD_NUM = 0, 따라서 if문에 걸리지 않고 다음 블럭으로 넘어감. 
            player_socket_num = accept(listen_socket,(struct sockaddr*)&addr, &struct_len); // 해당 사용자의 FD_NUM을 player_socket_num에 accpet() 인자로 전달한 addr에는 주소를 받음 
            for(int i=0;i<socket_max;++i){ 
                if(close_socket[player_socket_num] == 1){
                    close_socket[player_socket_num] = 0;
                }
            }
            if(player_socket_num == -1){error("accept error");}
            printf("new player\n");
            add_player(player_socket_num,&addr); // 사용자 추가 부분 => player_socket_num을 player 소켓 배열에 저장, 주소 저장 
            printf("player num : %d\n",player_num);
            _main(player_socket_num);
        } // end of connect check

        for(int i=0; i<player_num; ++i){
            if(FD_ISSET(player[i],&fds)){
                if(sign[player[i]] == 1) continue; // *** sign이 1인(이미 thread로 처리중인 소켓)은 pass 
                sign[player[i]]=1;
                pthread_create(&thread[player[i]],NULL,selecter,(void*)&player[i]);
                // 스레드를 생성하여 해당 클라이언트 소켓의 요청을 처리 
            }

        }
    }
}

// listen socket을 만드는 함수 
int  tcp_listen(int host, int port, int backlog) {
    int sd;
    struct sockaddr_in servaddr;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("socket fail");
        exit(1);
    }
    // servaddr 구조체의 내용 세팅
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(host);
    servaddr.sin_port = htons(port);
    if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind fail");  exit(1);
    }
    // 클라이언트로부터 연결요청을 기다림
    listen(sd, backlog);
    return sd;
}

//  가장 큰 소켓번호를 리턴, main에서 select함수에 사용됨
int max_socket_num(){
    int max = 0;
    max = listen_socket;
    for(int i=0;i<player_num;++i){
        if(max < player[i]) max = player[i];
    }
    return max + 1; // +1로 전달해야 원하는 길이까지 확인가능 
}

// 플레이어를 새로 추가하는 함수 
void add_player(int socket_num, struct sockaddr_in *client_addr){
    char buf[20]; // 주소를 저장할 buf 
    inet_ntop(AF_INET, &client_addr->sin_addr, buf, sizeof(buf)); // 네트워크 정보(빅 인디언 이진데이터) => AF_INET 프로토콜(IPV4)로 변환 
    player[player_num] = socket_num;
    user[socket_num].page = 1; // 처음 접속한 유저의 페이지를 1로 설정 
    player_num++;
}

// 메인 페이지 출력함수 
void _main(int socket_num){
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/main.txt",O_RDONLY);
    char tmp[10000];
    read(fd,tmp,10000-1);
    send(socket_num,tmp,strlen(tmp),0);
    bzero(tmp,sizeof(tmp));
}

// 방 정보 출력함수 
void _room(int s_n){
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_RDONLY);
    char tmp[BUFSIZ];
    read(fd,tmp,sizeof(tmp)-1);
    send(s_n,tmp,strlen(tmp),0);
    bzero(tmp,sizeof(tmp));
}

// 사용자가 현재 머물고 있는 page와 해당 page에서 선택한 옵션에 맞게 처리하는 함수
// ex) user.page = 2, select = 0 ==> page2_0 (방 만들기)
void* selecter(void* socket_num){
    char input[1000];
    int* tmp = (int*)socket_num;
    int s_n = *tmp;
    printf("[LOG] USER %s IN SELECTER\n",user[s_n].name);
    int page;
    int select;
    if(close_socket[s_n] == 1) return NULL;
    read(s_n,input,sizeof(input));

    page = user[s_n].page;
    select = atoi(input);

    printf("[LOG] USER %s  IN PAGE : %d CHOOSE %d\n",user[s_n].name,page,select);

    // selecter에서 나올 때는 sing bit를 0으로 하여 main에서 다시 사용자의 입력을 받을 수 있게 처리
    switch(page){
        case 1:
            page1(select, s_n);
            sign[s_n] = 0;
            break;
        case 2:
            page2(select,s_n);
            sign[s_n] = 0;
            break;
         
        case 3:
            page3(select,s_n);
            sign[s_n] = 0;
            break;
    }
}

// 페이지1 함수
// 선택한 옵션에 따른 함수를 불러줌 
void page1(int n,int s_n){
    switch(n){
        case 1:
            // 로그인 
            page1_1(s_n);
            break;
       case 2:
            // ID 찾기 
            page1_2(s_n);
            break;
       case 3:
            // PW 찾기
            page1_3(s_n);
            break; 
        case 4:
            // Sign-up
            page1_4(s_n);
            _main(s_n);
            break;
        case 0:
            // Exit 
            page1_0(s_n);
            break;
    }
}

//로그인
//login_id()를 통해 해당 id가 존재하는지 확인
void page1_1(int socket_num){
    int length;
    char tmp[1024];
    char t1[1024];
    char t2[1024] = "ID를 입력해주세요 \n";
    char t4[1024] = "해당 ID가 존재하지 않습니다.\n";
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/login_page.txt",O_RDONLY);
    read(fd,tmp,1024-1);
    send(socket_num,tmp,strlen(tmp),0);
    sprintf(t1,"현재 접속중인 인원 : %d\n",player_num);
    send(socket_num,t1,strlen(t1),0); 
    while(1){
        send(socket_num,t2,strlen(t2),0);
        if(login_id(socket_num) == 0){ 
            send(socket_num,t4,sizeof(t4),0);
            continue;
        }
        break;
    }

    // 로그인 성공 블록 
    user[socket_num].page = 2;
    user[socket_num].room = 0; // 아직 방에 들어가지 않았으므로 이를 표시하는 0으로 설정.
    user[socket_num].in_game = 0;
    _room(socket_num); // 방 정보 출력
}
    

// 입력한 id가 존재하는지 확인
// database에 유저 정보가 <id>~~~<pw>~~~<un>~~~ 의 형식으로 저장됨
// 따라서 tag를 활용하여 id,pw,un을 구분
int login_id(int socket_num){
    char id[1000] = "<id>";
    char tmp[1024];
    char input[1000];
    char null[3] = "\0";
    char cmp[1024];
    char cmp_id[] = "<id>";
    char cmp_pw[] = "<pw>";
    int gap;
    char* start;
    char* end;

    recv(socket_num,input,sizeof(input),0); // 사용자에게서 id  읽기 

    FILE* data = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt","r");

    // 입력 받은 id에 tag <id>를 붙임
    strncat(id,input,sizeof(id)-strlen(id)-1);
    strncat(id,null,sizeof(id)-strlen(id)-1);
   

    while(fgets(tmp,sizeof(tmp),data) != NULL){
        tmp[strlen(tmp)-1] = '\0';
        // database에서 읽어온 data에서 id부분만 추출
        // fgets로 읽어올 때 <id>~~~<pw>~~~<un>~~~ 의 형태로 데이터가 읽힘
        start = strstr(tmp,cmp_id);
        end = strstr(tmp,cmp_pw);
        gap = (end - start)/sizeof(char);
        strncpy(cmp,tmp,gap);

        // 아이디가 일치하면 PW를 검사해야 하므로 login_pw() 함수 호출 
        if(strcmp(id,cmp)==0) {
            login_pw(socket_num,tmp);
            fclose(data);
            bzero(input,sizeof(input));
            bzero(id,sizeof(id));
            bzero(tmp,sizeof(tmp));
            bzero(null,sizeof(null));
            bzero(cmp,sizeof(cmp));
            return 1;
        }
        bzero(tmp,sizeof(tmp));
        bzero(cmp,sizeof(cmp));
    } 
    fclose(data);
    bzero(input,sizeof(input));
    bzero(id,sizeof(id));
    bzero(tmp,sizeof(tmp));
    bzero(null,sizeof(null));
    bzero(cmp,sizeof(cmp));

    return 0;
}

// PW가 일치하는지 확인하는 함수 
// 방법은 login_id()와 거의 유사 
// 마무리 처리 로직이 다르므로 별도의 함수로 만듦. 
int login_pw(int socket_num, char* s){

    char t3[] = "PW를 입력해주세요 \n"; 
    char t5[] = "비밀번호를 다시 입력 해주세요.\n";
    char pwd[1000] = "<pw>";
    char tmp[1024];
    char cmp[1024];
    char input[100];
    char null[3] = "\0";
    char cmp_pw[] = "<pw>";
    char cmp_un[] = "<un>";
    char* start;
    char* end;
    int gap;
    char* un;
    
    send(socket_num,t3,strlen(t3),0);
    recv(socket_num,input,sizeof(input),0); // 사용자에게서 pw  읽기 
    
    strncat(pwd,input,sizeof(pwd)-strlen(pwd)-1);
    strncat(pwd,null,sizeof(pwd)-strlen(pwd)-1);
    start = strstr(s,cmp_pw);
    end = strstr(s,cmp_un);
    gap = (end-start)/sizeof(char);
    strncpy(cmp,start,gap);
    while(1){
        // 입력한 PW가 일치한 경우 user 정보에 닉네임을 붙여주고 리턴 1
        if(strcmp(cmp,pwd)==0){
            bzero(input,sizeof(input));
            un = strstr(s,"<un>");
            strcpy(user[socket_num].name,un+4); // 로그인한 유저의 닉네임 붙여주기
            return 1;
            bzero(t3,sizeof(t3));
            bzero(t5,sizeof(t5));
            bzero(pwd,sizeof(pwd));
            bzero(input,sizeof(input));
            bzero(null,sizeof(null));
        }
        //PW가 틀린 경우 다시 입력 받기 
        else{
            bzero(input,sizeof(input));
            bzero(pwd,sizeof(pwd));
            send(socket_num,t5,strlen(t5),0);
            recv(socket_num,input,sizeof(input),0);
            sprintf(pwd,"<pw>%s",input);
        }
    }
    bzero(t3,sizeof(t3));
    bzero(t5,sizeof(t5));
    bzero(pwd,sizeof(pwd));
    bzero(input,sizeof(input));
    bzero(null,sizeof(null));

    return 0;
}


// 회원가입 함수
// database에 유저 정보(id,pw,un)에 tag를 붙여서 저장해야 함.
void page1_4(int socket_num){
    char info[10000]; 
    char info_id[] = "ID를 입력해주세요\n";
    char info_id2[] = "중복되는 ID 입니다. 다른 ID를 입력해주세요\n";
    char info_pw[] = "PW를 입력해주세요\n";
    char info_un[] = "닉네임을 입력해주세요\n";
    char info_un2[] = "중복되는 닉네임 입니다. 다른 닉네임을 입력해주세요\n"; 
    char finish[] = "회원가입이 완료 되었습니다!\n";
    char id[100];
    char pw[100];
    char un[100];
    char id_tag[] = "<id>";
    char pw_tag[] = "<pw>";
    char un_tag[] = "<un>";
    char result[1000];
    
    // 유저 database 읽어오기
    int data = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt",O_RDWR | O_APPEND);
    char tmp[1024];
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/signup_page.txt",O_RDONLY);

    read(fd,info,sizeof(info)-1);
    write(socket_num,info,strlen(info));
    

    // 사용자에게 Id 입력받음 
    while(1){
        send(socket_num,info_id,strlen(info_id),0);
        recv(socket_num,tmp,sizeof(tmp),0);
        strcat(id,id_tag);
        strcat(id,tmp);
        // id 중복 체크 
        if(check_dupli_id(id)==1){ 
            strcpy(result,id);
            bzero(tmp,sizeof(tmp));
            bzero(id,sizeof(id));
            break;
        }
        // 중복되는 경우 다시 입력 받음 
        send(socket_num,info_id2,strlen(info_id2),0);
        bzero(id,sizeof(id));
        bzero(tmp,sizeof(tmp));
    }
    // 사용자에게 Pw 입력받음 
    send(socket_num,info_pw,strlen(info_pw),0);
    recv(socket_num,tmp,sizeof(tmp),0);
    strcat(pw,pw_tag);
    strcat(pw,tmp);
    strcat(result,pw);
    bzero(tmp,sizeof(tmp));

    // 사용자에게 닉네임 입력받음 
    while(1){
        send(socket_num,info_un,strlen(info_un),0);
        recv(socket_num,tmp,sizeof(tmp),0);
        tmp[strlen(tmp)] = '\n';
        strcat(un,un_tag);
        strcat(un,tmp);
        if(check_dupli_un(un)==1){
            strcat(result,un);
            write(data,result,strlen(result));
            bzero(un,sizeof(un));
            bzero(tmp,sizeof(tmp));
            break;
        }
        send(socket_num,info_un2,strlen(info_un2),0);
        bzero(un,sizeof(un));
        bzero(tmp,sizeof(tmp));
    }

    close(data);
    close(fd);
    
    send(socket_num,finish,strlen(finish),0);

    bzero(id,sizeof(id));
    bzero(pw,sizeof(pw));
    bzero(un,sizeof(un));
    bzero(info,sizeof(info));
    bzero(finish,sizeof(finish));
    bzero(result,sizeof(result));

}

// id_찾기 함수
// 닉네임으로 id를 찾음 
void page1_2(int s_n){
    FILE* fd = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt","r");
    char un[1000] = "<un>";
    char data[1000];
    char tmp[1000];
    char id[] = "<id>";
    char pw[] = "<pw>";
    char returns[1000];
    char info_1[] = "닉네임을 입력해주세요\n";
    char info_2[] = "찾으시는 ID : ";
    send(s_n,info_1,strlen(info_1),0);
    recv(s_n, data, sizeof(data),0);
    strcat(un,data);

    while(fgets(tmp,sizeof(tmp),fd) != NULL){
        tmp[strlen(tmp)-1] = '\0';
        char* p;
        char* g;
        int gap;
        if((strstr(tmp,un))!=NULL){
            // tag를 사용하여 database에서 아이디를 추출함 
            p = strstr(tmp,id);
            g = strstr(tmp,pw);
            gap = (g-p)/sizeof(char);
            strncat(returns,tmp+(4*sizeof(char)),gap-4);
            returns[strlen(returns)] = '\n';
            returns[strlen(returns)] = '\0';
            send(s_n,info_2,strlen(info_2),0);
            send(s_n,returns,strlen(returns),0);
            bzero(un,sizeof(un));
            bzero(data,sizeof(data));
            bzero(tmp,sizeof(tmp));
            bzero(id,sizeof(id));
            bzero(pw,sizeof(pw));
            bzero(returns,sizeof(returns));
            bzero(info_1,sizeof(info_1));
            bzero(info_2,sizeof(info_2));
            _main(s_n);
            return ;
        }
    }
    bzero(un,sizeof(un));
    bzero(data,sizeof(data));
    bzero(tmp,sizeof(tmp));
    bzero(id,sizeof(id));
    bzero(pw,sizeof(pw));
    bzero(returns,sizeof(returns));
    bzero(info_1,sizeof(info_1));
    bzero(info_2,sizeof(info_2));
    return;
}



// 비밀번호 찾기 함수
// id찾기와 같은 방법 사용, tag가 다르므로 별도의 함수로 만듦.
void page1_3(int s_n){
    FILE* fd = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt","r");
    char id[1000] = "<id>";
    char data[1000];
    char tmp[1000];
    char pw[] = "<pw>";
    char un[] = "<un>";
    char returns[1000];
    char info_1[] = "ID를 입력해주세요\n";
    char info_2[] = "찾으시는 PW : ";
    send(s_n,info_1,strlen(info_1),0);
    recv(s_n, data, sizeof(data),0);
    strcat(id,data);

    while(fgets(tmp,sizeof(tmp),fd) != NULL){
        tmp[strlen(tmp)-1] = '\0';
        char* p;
        char* g;
        int gap;
        if((strstr(tmp,un))!=NULL){
            // tag를 사용하여 pw를 추출
            p = strstr(tmp,pw);
            g = strstr(tmp,un);
            gap = (g-p)/sizeof(char);
            strncat(returns,p+(4*sizeof(char)),gap-4);
            returns[strlen(returns)] = '\n';
            returns[strlen(returns)] = '\0';
            send(s_n,info_2,strlen(info_2),0);
            send(s_n,returns,strlen(returns),0);
            bzero(un,sizeof(un));
            bzero(data,sizeof(data));
            bzero(tmp,sizeof(tmp));
            bzero(id,sizeof(id));
            bzero(pw,sizeof(pw));
            bzero(returns,sizeof(returns));
            bzero(info_1,sizeof(info_1));
            bzero(info_2,sizeof(info_2));
            _main(s_n);
            return ;
        }
    }
    bzero(un,sizeof(un));
    bzero(data,sizeof(data));
    bzero(tmp,sizeof(tmp));
    bzero(id,sizeof(id));
    bzero(pw,sizeof(pw));
    bzero(returns,sizeof(returns));
    bzero(info_1,sizeof(info_1));
    bzero(info_2,sizeof(info_2));
    return;
}

// 접속 종료 
void page1_0(int s_n){
    char info[] = "EXIT SERVER\n";
    send(s_n,info,strlen(info),0);
    player_num--;
    close_socket[s_n] = 1; // 스레드 중첩 방지를 위해 close_socket 1로 설정 -> FD_SET에서 제외될 수 있음
    close(s_n); // 연결 끊기
    bzero(info,sizeof(info));
}

// id 중복 확인 함수(회원가입)
int check_dupli_id(char* input){
    char* start;
    char* end;
    int gap;
    char tmp[1000];
    char cmp[1000];
    FILE* data = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt","r");
    // database에서 하나씩 읽어 id가 중복되는가 체크, tag활용
    while(fgets(tmp,sizeof(tmp),data) != NULL){
        tmp[strlen(tmp)-1] = '\0';
        start = strstr(tmp,"<id>");
        end = strstr(tmp,"<pw>");
        gap = (end-start)/sizeof(char);
        strncpy(cmp,tmp,gap);
        if(strcmp(cmp,tmp)==0){
            fclose(data);
            bzero(tmp,sizeof(tmp));
            return 0; // 중복된 경우 리턴 0 
        }
        bzero(tmp,sizeof(tmp));
        bzero(cmp,sizeof(cmp));
    }
    fclose(data);
    return 1; // 중복되지 않은 경우 리턴 1 
}

// 닉네임 중복 확인 함수(회원가입)
int check_dupli_un(char* input){
    char* start;
    int gap;
    char tmp[1000];
    char cmp[1000];
    FILE* data = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/database.txt","r");
    // database에서 하나씩 읽어와 중복을 확인, tag 활용
    while(fgets(tmp,sizeof(tmp),data) != NULL){
        start = strstr(tmp,"<un>");
        strcpy(cmp,start);
        if(strcmp(cmp,input)==0){
            fclose(data);
            bzero(tmp,sizeof(tmp));
            return 0; // 중복된 경우 리턴 0 
        }
        bzero(tmp,sizeof(tmp));
        bzero(cmp,sizeof(cmp));
    }
    fclose(data);
    return 1; // 중복되지 않은 경우 리턴 1 
}


// page2 일때 처리 함수 
void page2(int n,int s_n){ // n은 사용자가 선택한 옵션을 의미 
    char info[] = "올바르지 않은 입력입니다. 다시 입력해주세요\n";
    
    // 사용자가 잘못된 옵션을 입력한 경우(현존하는 방 번호보다 큰 숫자 or 등록되지 않은 음수) 예외처리
    if( n>room_num|| (n < -1 && n!=-5)){
        char tmp[100];
        while(1){
            send(s_n,info,strlen(info),0);
            recv(s_n,tmp,sizeof(tmp),0);
            n = atoi(tmp);
            if(n == -1){
                user[s_n].page = 1;
                _main(s_n);
                return;
            }
            if(n<=room_num && n > (-1)) break; // 올바른 정보 입력됨 
        }
    }
   
    // 옵션에 문제가 없는 경우 옵션에 따라 함수 부르기
    switch(n){
       case -1:
            // input : -1 => go to main page
            user[s_n].page = 1;
            _main(s_n);
            return;
       case 0: // creat room
            page2_0(s_n);
            break;
       case -5: // 방목록 새로 고침
            page2_r(s_n);
            break;
       default : // join room
            page2_n(s_n,n);
            break;

    }
}

// 생성된 방의 정보를 만들어 파일에 저장하는 함수
// 추후 방의 정보를 파일에서 불러와 사용
void make_room(int s_n,int r_n){
    FILE *fp;
    char route[1000];
    char inter1[] = "=====================================================\n";
    //char head[] = "HEAD COUNT : 1 (Game needs 5 members)\n"
    char start[] = "START GAME : PLEASE ENTER '1'\n";
    char quit[] = "QUIT ROOM : PLEASE ENTER '0'\n";
    sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",r_n);
    fp = fopen(route,"w");
   
    // 파일에 방 정보 입력
    fwrite(inter1,sizeof(char),strlen(inter1),fp);
    fprintf(fp,"            %s\n",rooms[r_n].name);
    fprintf(fp,"%s\n\n",inter1);
    fprintf(fp,"HEAD COUNT : %d (Game needs 5 members)\n",rooms[r_n].head);
    fwrite(start,sizeof(char),strlen(start),fp);
    fwrite(quit,sizeof(char),strlen(quit),fp);
    fprintf(fp,"%s\n",inter1);

    for(int i=1;i<=rooms[r_n].head;++i) fprintf(fp,"%d. %s\n",i,user[rooms[r_n].u_s[i-1]].name);
    
    bzero(route,sizeof(route));
    bzero(inter1,sizeof(inter1));
    bzero(start,sizeof(start));
    bzero(quit,sizeof(quit));
    
    fclose(fp);
}
    


// 유저가 방을 생성하는 함수 
void page2_0(int s_n){
    int r_n = room_num; // 타 클라이언트가 동시에 방을 생성하면 룸 넘버가 겹칠 수 있기 때문에 room_creat 함수에 들어오는 순간 바로 저장 
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_RDWR | O_APPEND); // room list에 대한 정보를 저장하고 있는 파일 
    int wd;
    char input[500];
    char room_name[1000];
    char file_name[1000];
    char route[1000];
    char info[] = "생성할 방 제목을 입력하세요\n";
    char room_info[10000];
    send(s_n,info,strlen(info),0);
    recv(s_n,input,sizeof(input),0);
    
    strcpy(rooms[r_n].name,input);
    
    sprintf(room_name,"\n%d. %s",r_n, input); // 파일에 들어갈 방 제목 정보 
    write(fd,room_name,strlen(room_name)); // room_list에 생성된 방의 정보 추가 
    rooms[r_n].head = 1; // 방 인원 1명으로 세팅 
    strncpy(rooms[r_n].u_n[0],user[s_n].name,sizeof(user[s_n].name));
    rooms[r_n].u_s[0] = s_n;
    user[s_n].room = r_n; // 유저가 현재 r_n 번 방에 들어갔음을 유저 정보에 등록 
    make_room(s_n,r_n);
    room_num++; // 방 번호 +1 
    sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",r_n); // 방 정보를 등록할 경로 생성 
    wd = open(route,O_RDONLY);
    read(wd,room_info,sizeof(room_info));
    send(s_n,room_info,strlen(room_info),0);
    user[s_n].page = 3; // 방 page 인 3 page로 유저를 보냄 

    bzero(input,sizeof(input));
    bzero(room_name,sizeof(room_name));
    bzero(file_name,sizeof(file_name));
    bzero(route,sizeof(route));
    bzero(info,sizeof(info));
    bzero(room_info,sizeof(room_info));
    close(fd);
    close(wd);
}

// 유저가 생성된 방에 입장하는 함수
// s_n : 소켓 넘버
// n: 들어가고자 하는 방 넘버 
void page2_n(int s_n,int n){
    char info[] = "방에 사람이 가득 찼습니다!";
    int fd;
    char route[1000];
    char room_info[5000];
   
    // 방이 꽉찬 경우 예외처리 
    if(rooms[n].head==5){ 
        write(s_n,info,strlen(info));
        return;
    }
    // 유저가 방에 들어온 경우
    // 방 정보 업데이트
    user[s_n].room = n; // 유저 정보에 현재 있는 방 표시 
    rooms[n].head++;
    strcpy(rooms[n].u_n[rooms[n].head-1],user[s_n].name);
    rooms[n].u_s[rooms[n].head-1] = s_n;
    make_room(s_n,n);
    sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",n);
    fd = open(route,O_RDONLY);
    read(fd,room_info,sizeof(room_info));

    // 방에 있는 모든 멤버에게 업데이트된 방 정보를 출력 
    for(int i=0;i<rooms[n].head;++i) send(rooms[n].u_s[i],room_info,strlen(room_info),0);
    user[s_n].page = 3;

    bzero(info,sizeof(info));
    bzero(route,sizeof(route));
    bzero(room_info,sizeof(room_info));
}

// 방 새로고침 함수 
void page2_r(int s_n){
    char info[1000];
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_RDONLY);
    
    read(fd,info,sizeof(info));
    send(s_n,info,strlen(info),0);
    bzero(info,sizeof(info));
}


// 방 목록 초기화 함수
// 서버가 다시 시작할 때 사용 
void reset_room(){
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_WRONLY|O_CREAT|O_TRUNC);
    char info1[] = "=====================================================\n";
    char info2[] = "                    ROOM LIST                        \n";
    char info3[] = "CREAT ROOM : 0\n";
    char info4[] = "ENTER ROOM : INSERT ROOM NUMBER\n";
    char info5[] = "REFRESH : -5\n";
    char info6[] = "EXIT : -1\n";
    write(fd,info1,strlen(info1));
    write(fd,info2,strlen(info2));
    write(fd,info1,strlen(info1));
    write(fd,info3,strlen(info3));
    write(fd,info4,strlen(info4));
    write(fd,info5,strlen(info5));
    write(fd,info6,strlen(info6));
    write(fd,info1,strlen(info1));
    bzero(info1,sizeof(info1));
    bzero(info2,sizeof(info2));
    bzero(info3,sizeof(info3));
    bzero(info4,sizeof(info4));
    bzero(info5,sizeof(info5));
    bzero(info6,sizeof(info6));
    close(fd);
}    

// page3 처리 함수 
// n : 옵션
// s_n : 소켓 넘버 
void page3(int n, int s_n){
    int r_n = user[s_n].room;

    switch(n){
        case 1:
            // 게임시작 
            page3_1(s_n);
            break;
        case 0:
            // 방에서 나가기 
            page3_0(s_n);
            break;
    }
}



// 게임 시작
void page3_1(int s_n){
    char info[] = "5명이 되어야 게임을 시작할 수 있습니다!\n";
    char info2[] = "3초 뒤 게임을 시작합니다.\n";
    int r_n = user[s_n].room;
    int head = rooms[r_n].head;
    pthread_t thread_id[5]; // 각 클라이언트 소켓마다 스레드를 생성할 것이므로 스레드 배열 선언 

    // 방 인원이 5명이 아닌 경우 예외처리 
    if(head!=5){
        send(s_n,info,strlen(info),0);
        return;
    }
    
    // 게임 시작 알림 
    for(int i=0;i<rooms[r_n].head;++i) send(rooms[r_n].u_s[i],info2,strlen(info2),0);
    sleep(1);
    for(int i=0;i<rooms[r_n].head;++i) send(rooms[r_n].u_s[i],"3",2,0);
    sleep(1);
    for(int i=0;i<rooms[r_n].head;++i) send(rooms[r_n].u_s[i],"2",2,0);
    sleep(1);
    for(int i=0;i<rooms[r_n].head;++i) send(rooms[r_n].u_s[i],"1",2,0);
    
    for(int i=0;i<rooms[r_n].head;++i) user[rooms[r_n].u_s[i]].page = 4; // game page 4;

    // 게임에 들어가기 직전 세팅 
    game_rooms[r_n].num_clients = 5;
    game_rooms[r_n].votes_received = 0; 

    // 게임 데이터 세팅(소켓번호, 닉네임, in_game bit 활성화, sign bit 활성화)
    // in_game과 sign bit을 활성화 하여 스레드 중첩을 막음
    for(int i =0;i<5;++i) game_rooms[r_n].vote_counts[i]=0;
    for(int i =0;i<5;++i){
        game_rooms[r_n].clients[i].sock = rooms[r_n].u_s[i];
        game_rooms[r_n].clients[i].r_n = r_n;
        strcpy(game_rooms[r_n].clients[i].nickname,rooms[r_n].u_n[i]);
        sign[rooms[r_n].u_s[i]] = 1;
        user[game_rooms[r_n].clients[i].sock].in_game = 1; 
    }

    // 게임에 시작된 방은 방목록에서 제거 
    change_room_list(r_n);


    // 게임시작 

    // (1) 클라이언트 스레드
    // (2) game_loop
    // 두 가지를 활용하여 게임 진행 

    // (1) 에서는 개별 클라이언트의 입력 정보를 처리
    // (2) 에서는 게임 진행에서의 안내 메세지를 처리 

    // 모든 게임 멤버마다 별도의 스레드를 생성. 각 스레드를 컨트롤하여 게임 진행 
    for(int i=0;i<rooms[r_n].head;++i) pthread_create(&thread_id[i], NULL, client_handler, (void *)&game_rooms[r_n].clients[i]);
    
    game_loop(thread_id,r_n);

    
    return;
}


// 방에서 나가기 
void page3_0(int s_n){
    int fd;
    int r_n = user[s_n].room;
    int head = rooms[r_n].head;
    char route[1000];
    char room_info[1000];


    // 유저가 5번째 유저라면 head만 줄이고 나감 
    if(rooms[r_n].u_s[head-1] == s_n){

        // 유저가 나갔으므로 방 정보 업데이트 
        bzero(rooms[r_n].u_n[head-1],sizeof(rooms[r_n].u_n[head-1]));
        rooms[r_n].head--;
        make_room(s_n,r_n);
        sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",r_n);
        fd = open(route,O_RDONLY);
        read(fd, room_info, sizeof(room_info));

        // 업데이트 된 방 정보를 방의 클라이언트에게 송신 
        for(int i=0;i<head-1;++i) send(rooms[r_n].u_s[i],room_info,strlen(room_info),0);
        user[s_n].page = 2;
        page2_r(s_n);
        close(fd);
        
        // 만약 나간 유저가 마지막 유저라면 방 목록에서 해당 방의 정보를 삭제 
        if(rooms[r_n].head == 0){
            sprintf(route,"/rooms/%d.txt",r_n);
            remove(route);
            bzero(route,sizeof(route));
            bzero(room_info,sizeof(room_info));
            change_room_list(r_n);
            user[s_n].page = 2;
            page2_r(s_n);
            return;    
        }
    }

    // 유저가 5번째 유저가 아닌 경우 
    for(int i=0;i<head-1;++i){
        // 나가려는 유저의 넘버를 찾기 
        if(rooms[r_n].u_s[i] != s_n) continue;

        // 나가려는 유저의 th를 찾은 경우 
        // 방 정보 업데이트 
        for(int j=i;j<head-1;++j){
            bzero(rooms[r_n].u_n[j],sizeof(rooms[r_n].u_n[j]));
            strcpy(rooms[r_n].u_n[j],rooms[r_n].u_n[j+1]);
            rooms[r_n].u_s[j] = rooms[r_n].u_s[j+1];
        }
        rooms[r_n].head--;
        make_room(s_n,r_n);
        sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",r_n);
        fd = open(route,O_RDONLY);
        read(fd, room_info, sizeof(room_info));

        // 업데이트 된 방 정보를 방에 존재하는 클라이언트에게 송신 
        for(int k=0;k<head-1;++k) send(rooms[r_n].u_s[k],room_info,strlen(room_info),0);
        close(fd);
        
        user[s_n].page = 2;
        page2_r(s_n);
        bzero(route,sizeof(route));
        bzero(room_info,sizeof(room_info));
        break;         
    }

    // 만약 나간 클라이언트가 마지막인 경우 
    // 방 정보를 방 목록에서 삭제 
    if(rooms[r_n].head == 0){
            sprintf(route,"/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/rooms/%d.txt",r_n);
            remove(route);
            bzero(route,sizeof(route));
            bzero(room_info,sizeof(room_info));
            change_room_list(r_n);
            user[s_n].page = 2;
            page2_r(s_n);
            return;    
    }
}

// 방 목록 정보 변경 함수
// 방이 삭제되는 경우(마지막 멤버가 방에서 나감, 방이 게임에 들어감) 방 목록에서 해당 방 삭제
void change_room_list(int r_n){
    rooms[r_n].del = 1;
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_template.txt",O_RDONLY);
    FILE* fp = fopen("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt","w");
    char data[1000];
    bzero(data,sizeof(data));
    read(fd,data,sizeof(data));
    fprintf(fp,"%s",data);
    for(int i=1;i<=room_num-1;++i){
        if(rooms[r_n].del==1) continue;
        fprintf(fp,"%d. %s\n",i,rooms[i].name);
    }
    room_num;
    close(fd);
    fclose(fp);
    bzero(data,sizeof(data));
}

// 아래 부터는 'In game' 관련 코드
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void send_to_all_clients(int r_n, char *msg) {
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (!game_rooms[r_n].clients[i].is_dead) {  // 살아있는 클라이언트에게만 전송
            write(game_rooms[r_n].clients[i].sock, msg, strlen(msg));
        }
    }
}

void assign_roles(int r_n) {
    int roles[MAX_USERS] = {0, 1, 2, 0, 0}; // 1명의 마피아, 1명의 경찰, 3명의 시민

    // 역할을 무작위로 섞기
    for (int i = 0; i < 5; i++) {
        int rand_idx = rand() % MAX_USERS;
        int temp = roles[i];
        roles[i] = roles[rand_idx];
        roles[rand_idx] = temp;
    }

    // 각 클라이언트에게 역할 할당
    for (int i = 0; i < 5; i++) {
        game_rooms[r_n].clients[i].role = roles[i];
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
        write(game_rooms[r_n].clients[i].sock, msg, strlen(msg));
    }
}

void start_discussion(int r_n) {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "\n===== Discussion Time =====\n[사회자] 토론 시간입니다. (60초)\n");
    send_to_all_clients(r_n,msg);
    
    // 60초 토론 시간
    sleep(60);

    snprintf(msg, sizeof(msg), "\n[사회자] 토론 시간이 종료되었습니다. 투표를 시작합니다.\n");
    send_to_all_clients(r_n,msg);
}
void process_voting_result(int r_n, int eject_index) {
    char msg[BUF_SIZE];

    if (eject_index == -1) {
        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: 아무도 추방되지 않았습니다.\n");
        send_to_all_clients(r_n,msg);
    } else {
        game_rooms[r_n].clients[eject_index].is_dead = 1;
        game_rooms[r_n].player_status[eject_index].is_dead = 1;

        snprintf(msg, sizeof(msg), "[사회자] 투표 결과: %s님이 추방되었습니다.\n",
                game_rooms[r_n].clients[eject_index].nickname);
        send_to_all_clients(r_n,msg);

        // 추방된 사람에게도 결과 메시지 전송
        snprintf(msg, sizeof(msg), "[사회자] 당신은 추방되었습니다.\n");
        write(game_rooms[r_n].clients[eject_index].sock, msg, strlen(msg));
        

        if (game_rooms[r_n].clients[eject_index].role == 1) {
            snprintf(msg, sizeof(msg), "[사회자] 추방된 플레이어는 마피아였습니다!\n");
        } else {
            snprintf(msg, sizeof(msg), "[사회자] 추방된 플레이어는 마피아가 아니었습니다.\n");
        }
        send_to_all_clients(r_n,msg);
    }
}



void end_voting(int r_n) {
    int max_votes = 0;
    int eject_index = -1;
    int max_vote_count = 0;

    // 투표 결과 집계
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (game_rooms[r_n].vote_counts[i] > max_votes) {
            max_votes = game_rooms[r_n].vote_counts[i];
            eject_index = i;
            max_vote_count = 1;
        } else if (game_rooms[r_n].vote_counts[i] == max_votes && max_votes > 0) {
            max_vote_count++;
        }
    }

    // 투표 결과 처리
    if (max_vote_count > 1) {
        // 동률인 경우 아무도 추방되지 않음
        process_voting_result(r_n,-1);
    } else {
        // 한 명이 추방됨
        process_voting_result(r_n,eject_index);
    }
}
 int get_alive_players_count(int r_n) {
    int count = 0;
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (!game_rooms[r_n].player_status[i].is_dead) {
            count++;
        }
    }
    return count;
}

void voting(int r_n) {
    printf("[DEBUG] 투표 시작\n");

    // 투표 변수 초기화
    memset(game_rooms[r_n].vote_counts, 0, sizeof(game_rooms[r_n].vote_counts));
    game_rooms[r_n].votes_received = 0;

    char msg[BUF_SIZE];
    char voting_msg[BUF_SIZE];
    snprintf(msg, sizeof(msg), "=== 투표를 시작합니다! ===\n투표를 진행하세요! 누구를 추방할까요? (1~%d 중 번호를 입력하세요)\n", game_rooms[r_n].num_clients);
    send_to_all_clients(r_n,msg);

    // 투표 대상 목록 출력
    memset(voting_msg, 0, sizeof(voting_msg));
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (!game_rooms[r_n].player_status[i].is_dead) {  // 살아있는 플레이어만 표시
            snprintf(voting_msg + strlen(voting_msg), sizeof(voting_msg) - strlen(voting_msg),
                     "%d. %s\n", i + 1, game_rooms[r_n].clients[i].nickname);
        }
    }
    send_to_all_clients(r_n,voting_msg);
    printf("[DEBUG] 투표 목록 전송 완료\n");

    // 투표 모드 활성화
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (!game_rooms[r_n].player_status[i].is_dead) {
            game_rooms[r_n].clients[i].in_voting = 1;
            game_rooms[r_n].clients[i].voted_for = -1;
        }
    }

    // 투표 처리
    while (game_rooms[r_n].votes_received < get_alive_players_count(r_n)) {
        printf("[DEBUG] 현재 투표 수: %d, 필요 투표 수: %d\n", game_rooms[r_n].votes_received, get_alive_players_count(r_n));
        sleep(2);  // CPU 사용률 감소를 위한 짧은 대기
    }

    printf("[DEBUG] 투표 종료, 결과 처리 시작\n");
    end_voting(r_n);
}

void night_mode(int r_n) {
    char msg[BUF_SIZE];
    int mafia_choice = -1;

    // 밤이 시작됨을 알림
    snprintf(msg, sizeof(msg), "\n===== Night Time =====\n[사회자] 밤이 되었습니다.\n");
    send_to_all_clients(r_n,msg);

    // 경찰 차례 알림
    snprintf(msg, sizeof(msg), "[사회자] 경찰이 조사를 시작합니다.\n");
    send_to_all_clients(r_n,msg);

    // 경찰 턴 처리
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (game_rooms[r_n].clients[i].role == 2 && !game_rooms[r_n].clients[i].is_dead) {
            char player_list[BUF_SIZE] = "=== 조사할 플레이어 선택 ===\n";
            for (int j = 0; j < game_rooms[r_n].num_clients; j++) {
                if (!game_rooms[r_n].clients[j].is_dead && i != j) {
                    snprintf(player_list + strlen(player_list),
                            BUF_SIZE - strlen(player_list),
                            "%d. %s\n", j + 1, game_rooms[r_n].clients[j].nickname);
                }
            }
            strcat(player_list, "번호를 입력하세요: ");
            write(game_rooms[r_n].clients[i].sock, player_list, strlen(player_list));

            // 경찰을 밤 모드로 설정
            game_rooms[r_n].clients[i].in_night_mode = 1;
            game_rooms[r_n].clients[i].voted_for = -1;

            // 경찰의 선택을 기다림
            while (game_rooms[r_n].clients[i].in_night_mode) {
                usleep(100000);  // 0.1초 대기
            }

            int police_choice = game_rooms[r_n].clients[i].voted_for;
            if (police_choice >= 0 && police_choice < game_rooms[r_n].num_clients) {
                char result[BUF_SIZE];
                const char* role_str;

                // 역할을 문자열로 변환
                switch(game_rooms[r_n].clients[police_choice].role) {
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

                if (game_rooms[r_n].clients[police_choice].role == 1) {
                    snprintf(result, sizeof(result),
                            "[조사 결과] %s님은 마피아입니다.\n"
                            "조사 대상의 역할: %s\n",
                            game_rooms[r_n].clients[police_choice].nickname, role_str);
                } else {
                    snprintf(result, sizeof(result),
                            "[조사 결과] %s님은 마피아가 아닙니다.\n"
                            "조사 대상의 역할: %s\n",
                            game_rooms[r_n].clients[police_choice].nickname, role_str);
                }
                write(game_rooms[r_n].clients[i].sock, result, strlen(result));

                // 경찰의 조사가 끝났음을 알림
                snprintf(msg, sizeof(msg), "[사회자] 경찰이 조사를 마쳤습니다.\n");
                send_to_all_clients(r_n,msg);
            }
        }
    }

    // 마피아 차례 알림
    snprintf(msg, sizeof(msg), "[사회자] 마피아가 희생자를 선택합니다.\n");
    send_to_all_clients(r_n,msg);

    // 마피아 턴 처리
    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (game_rooms[r_n].clients[i].role == 1 && !game_rooms[r_n].clients[i].is_dead) {
            char player_list[BUF_SIZE] = "=== 희생자 선택 ===\n선택 가능한 플레이어:\n";
            for (int j = 0; j < game_rooms[r_n].num_clients; j++) {
                if (!game_rooms[r_n].clients[j].is_dead && i != j) {
                    snprintf(player_list + strlen(player_list),
                            BUF_SIZE - strlen(player_list),
                            "%d. %s\n", j + 1, game_rooms[r_n].clients[j].nickname);
                }
            }
            strcat(player_list, "번호를 입력하세요: ");
            write(game_rooms[r_n].clients[i].sock, player_list, strlen(player_list));

            // 마피아를 밤 모드로 설정
            game_rooms[r_n].clients[i].in_night_mode = 1;
            game_rooms[r_n].clients[i].voted_for = -1;

            // 마피아의 선택을 기다림
            while (game_rooms[r_n].clients[i].in_night_mode) {
                usleep(100000);  // 0.1초 대기
            }

            mafia_choice = game_rooms[r_n].clients[i].voted_for;
            if (mafia_choice >= 0 && mafia_choice < game_rooms[r_n].num_clients) {
                // 마피아가 선택을 완료했음을 알림
                snprintf(msg, sizeof(msg), "[사회자] 마피아가 선택을 마쳤습니다.\n");
                send_to_all_clients(r_n,msg);
                break;
            }
        }
    }

    // 밤이 끝나감을 알림
    snprintf(msg, sizeof(msg), "[사회자] 밤이 끝나갑니다...\n");
    sleep(2);  // 극적인 효과를 위한 짧은 대기
    send_to_all_clients(r_n,msg);

    // 밤 결과 처리
    if (mafia_choice != -1) {
        game_rooms[r_n].clients[mafia_choice].is_dead = 1;
        game_rooms[r_n].player_status[mafia_choice].is_dead = 1;
        snprintf(msg, sizeof(msg),
                "\n===== Morning =====\n[사회자] %s님이 밤사이 사망하셨습니다.\n",
                game_rooms[r_n].clients[mafia_choice].nickname);
        send_to_all_clients(r_n,msg);
    } else {
        send_to_all_clients(r_n,"\n===== Morning =====\n[사회자] 아무도 사망하지 않았습니다.\n");
    }
}
int check_game_end(int r_n) {
    int mafia_count = 0;
    int citizen_count = 0;

    for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
        if (!game_rooms[r_n].clients[i].is_dead) {
            if (game_rooms[r_n].clients[i].role == 1) {
                mafia_count++;
            } else {
                citizen_count++;
            }
        }
    }

    if (mafia_count == 0) {
                // 시민 승리 메시지, 모든 클라이언트에게 전송
        for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
            if (!game_rooms[r_n].clients[i].is_dead) {
                write(game_rooms[r_n].clients[i].sock, "\n[사회자] 시민 팀이 승리했습니다!\n게임에서 나가시려면 EXIT를 입력해주세요.\n", 
                        strlen("\n[사회자] 시민 팀이 승리했습니다!\n게임에서 나가시려면 EXIT를 입력해주세요.\n"));
                            } 
            else {
                // 사망한 플레이어에게도 결과 전송
                write(game_rooms[r_n].clients[i].sock, "\n[사회자] 시민 팀이 승리했습니다! (게임 종료)\n게임에서 나가시려면 EXIT를 입력해주세요.\n", 
                        strlen("\n[사회자] 시민 팀이 승리했습니다! (게임 종료)\n게임에서 나가시려면 EXIT를 입력해주세요.\n"));
            }
        }
        game_rooms[r_n].game_status=0;
        return 1;
    } else if (mafia_count >= citizen_count) {
        char room_info[1000];
        int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_RDONLY);
        read(fd,room_info,sizeof(room_info));

        // 마피아 승리 메시지, 모든 클라이언트에게 전송
        for (int i = 0; i < game_rooms[r_n].num_clients; i++) {
            if (!game_rooms[r_n].clients[i].is_dead) {
                write(game_rooms[r_n].clients[i].sock, "\n[사회자] 마피아가 승리했습니다!\n게임에서 나가시려면 EXIT를 입력해주세요.\n",
                        strlen("\n[사회자] 마피아가 승리했습니다!\n게임에서 나가시려면 EXIT를 입력해주세요.\n"));
                
            } else {
                // 사망한 플레이어에게도 결과 전송
                write(game_rooms[r_n].clients[i].sock, "\n[사회자] 마피아가 승리했습니다! (게임 종료)\n게임에서 나가시려면 EXIT를 입력해주세요.\n",
                        strlen("\n[사회자] 마피아가 승리했습니다! (게임 종료)\n게임에서 나가시려면 EXIT를 입력해주세요.\n"));
                
            }
        }
        game_rooms[r_n].game_status=0;
        return 1;
    }

    return 0;
}

void *client_handler(void *arg) {
    struct Clients *client = (struct Clients *)arg;
    char msg[BUF_SIZE];
    int len;
    char exit[] = "EXIT";
    int r_n = client -> r_n;
    char room_info[1000];
    int fd = open("/Users/seon/Desktop/server_program/ST_SeverProgramming/interface/room_list.txt",O_RDONLY);
    read(fd,room_info,sizeof(room_info));


    while (1) {
        bzero(msg,sizeof(msg));
        len = read(client->sock, msg, sizeof(msg) - 1);

        if(strcmp(msg,exit)==0) break;
        msg[len] = '\0';
        msg[strcspn(msg, "\n")] = 0;

        if (client->in_night_mode) {
            // 밤 모드 투표 처리
            int choice = atoi(msg) - 1;
            if (choice >= 0 && choice < game_rooms[r_n].num_clients) {
                // 선택 완료 메시지를 클라이언트에게 보냄
                write(client->sock, "선택이 완료되었습니다.\n", strlen("선택이 완료되었습니다.\n"));
                client->voted_for = choice;
                client->in_night_mode = 0;  // 투표 완료 후 밤 모드 해제
            }
        }
        else if (client->in_voting) {
            // 일반 투표 처리
            int vote = atoi(msg) - 1;
            if (vote >= 0 && vote < game_rooms[r_n].num_clients) {
                pthread_mutex_lock(&game_rooms[r_n].lock);
                if (client->voted_for == -1) {
                    game_rooms[r_n].vote_counts[vote]++;
                    client->voted_for = vote;
                    game_rooms[r_n].votes_received++;

                    char vote_msg[BUF_SIZE];
                    snprintf(vote_msg, sizeof(vote_msg), "[알림] %s님이 투표하셨습니다.\n", client->nickname);
                    client->in_voting = 0;
                    send_to_all_clients(r_n,vote_msg);
                }
                pthread_mutex_unlock(&game_rooms[r_n].lock);
            }
        } else {
            // 일반 채팅 메시지 처리
            if (!client->is_dead) {
                char chat_msg[1500];
                snprintf(chat_msg, sizeof(chat_msg), "[%s]: %s\n", client->nickname, msg);
                send_to_all_clients(r_n,chat_msg);
            }
        }   
    }
    user[client->sock].page = 2; // 방 목록으로 보내기
    sign[client->sock] = 0;
    user[client->sock].in_game = 0; 
    write(client->sock,room_info,strlen(room_info));
    
    return NULL;
}
void game_loop(pthread_t thread_id[5],int r_n) {
    // 게임 초기화
    memset(game_rooms[r_n].player_status, 0, sizeof(game_rooms[r_n].player_status));

    // 역할 배정
    assign_roles(r_n);

    while (1) {
        start_discussion(r_n);    // 낮 토론
        if (check_game_end(r_n)) break;

        voting(r_n);             // 투표
        if (check_game_end(r_n)) break;

        night_mode(r_n);         // 밤 모드
        if (check_game_end(r_n)) break;
    }
}



