/**    chat_server **/

// 필요한 헤더 파일 선언
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>
#include<time.h>

// 버퍼 크기, 최대 클라이언트 수, IP 주소 길이 정의
// 단위는 Byte
#define BUF_SIZE 200
#define MAX_CLNT 100
#define MAX_IP 30

// 함수 프로토타입 선언
void * handle_clnt(void *arg);
void send_msg(char *msg, int len);
void error_handling(char *msg);
char* serverState(int count);
void menu(char port[]);
void send_client_names(int clnt_sock);

/****************************/

// 클라이언트 정보 저장할 구조체 정의
typedef struct {
    int sock;                  // 클라이언트 소켓
    char name[BUF_SIZE];      // 클라이언트 이름
} ClientName;

ClientName clnt_socks[MAX_CLNT]; // 클라이언트 이름 저장 배열
int clnt_cnt=0; // 현재 클라이언트 수
pthread_mutex_t mutx; //뮤텍스

int main(int argc, char *argv[])
{
    // 서버 소켓과 클라이언트 소켓 선언
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    pthread_t t_id;
    
    /** time log **/
    struct tm *t;
    time_t timer = time(NULL);
    t=localtime(&timer);
 
    //서버 초기 설정
    if (argc != 2)
    {
        printf(" Usage : %s <port>\n", argv[0]);
        exit(1);
    }
 
    menu(argv[1]);
    pthread_mutex_init(&mutx, NULL);

    // 서버 소켓 생성 및 바인딩
    serv_sock=socket(PF_INET, SOCK_STREAM, 0);
    
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family=AF_INET;
    serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_adr.sin_port=htons(atoi(argv[1]));
 
    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
        error_handling("bind() error");
    if (listen(serv_sock, 5)==-1)
        error_handling("listen() error");
    
    // 클라이언트 연결 요청을 기다리고 수락하는 while문
    while(1)
    {
        timer = time(NULL); // 연결마다 시간 갱신
        t=localtime(&timer);
        clnt_adr_sz=sizeof(clnt_adr);

        // 클라이언트 연결 수락
        clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
        
        // 클라이언트 소켓과 정보를 구조체로 넘김
        ClientName *clnt = (ClientName *)malloc(sizeof(ClientName));
        clnt->sock = clnt_sock;
        
         // 클라이언트와 통신을 담당할 스레드 생성
        pthread_create(&t_id, NULL, handle_clnt, (void*)clnt);
        pthread_detach(t_id);
        printf(" Connected client IP : %s ", inet_ntoa(clnt_adr.sin_addr));
        /* 
        실제 출력하는걸 보면 127.0.0.1로 출력되는데, 
        이는 루프백(loopback)이여서 로컬 컴퓨터를 가르김
        네트워크를 통하지 않으며 자신에게 메시지를 보내는데 사용됨
        */
        printf("(%d-%d-%d %d:%d)\n", t->tm_year+1900, 
            t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min);
        printf(" chatter (%d/100)\n", (clnt_cnt+1));    
    }
    // 서버 종료 시 자원 정리
    close(serv_sock);
    return 0;
}

// handle_clnt 함수: 클라이언트와의 통신을 처리
void *handle_clnt(void *arg)
{
    ClientName* clnt = (ClientName*)arg; // 클라이언트 정보를 담고 있는 구조체 포인터
    int clnt_sock = clnt->sock; // 클라이언트 소켓 번호
    int str_len = 0, i;
    char msg[BUF_SIZE]; // 이름과 메시지를 결합할 버퍼

    /** time log **/
    struct tm *t;
    time_t timer = time(NULL);
    t = localtime(&timer);

    // 클라이언트 이름을 받아오기
    str_len = read(clnt_sock, clnt->name, sizeof(clnt->name)-1);
    if(str_len < 0) {
        error_handling("read() error");
    }
    clnt->name[str_len] = '\0'; // null-terminator 추가
    
    // 메시지 버퍼 초기화
    memset(msg, 0, BUF_SIZE);

    // 클라이언트 배열에 추가
    pthread_mutex_lock(&mutx); // 다른 스레드와의 동시 접근 방지를 위해 뮤텍스 잠금
    clnt_socks[clnt_cnt++] = *clnt; // 클라이언트 정보를 전역 배열에 추가
    pthread_mutex_unlock(&mutx); // 뮤텍스 잠금 해제

    // 다른 클라이언트에게 입장 메시지를 보내는 코드
    for(i = 0; i < clnt_cnt; i++) {
        if(clnt_socks[i].sock != clnt_sock) { // 자신을 제외한 다른 클라이언트에게만 메시지를 보냄
            write(clnt_socks[i].sock, msg, strlen(msg));
        }
    }

    // 클라이언트 Chat Name 리스트
    // 클라이언트로부터 메시지를 받고 처리하는 while문
    while((str_len = read(clnt_sock, msg, sizeof(msg))) != 0) {
        // "LIST_CLIENTS" 요청 확인
        if(strncmp(msg, "LIST_CLIENTS", 12) == 0) {
            memset(msg, 0, BUF_SIZE); // msg 버퍼 초기화
            send_client_names(clnt_sock);
        } 
        if(strncmp(msg, "NAME_CHANGE", 11) == 0) {
        // 이름 변경 요청 처리
        char* new_name = msg + 12; // "NAME_CHANGE " 이후의 문자열이 새 이름
        new_name[strcspn(new_name, "\n")] = 0; // 개행 문자 제거

        pthread_mutex_lock(&mutx);
        for(int i = 0; i < clnt_cnt; i++) {
            if(clnt_socks[i].sock == clnt_sock) {
                strcpy(clnt_socks[i].name, new_name); // 이름 업데이트
                break;
            }
        }
        pthread_mutex_unlock(&mutx);
        }else {
            // 그 외 메시지는 모든 클라이언트에게 전송
            send_msg(msg, str_len);
        }
    }

    timer = time(NULL); // 현재 시간을 다시 가져옴
    t = localtime(&timer); // 가져온 시간을 지역 시간으로 변환

    printf(" 사용자가 채팅방을 떠났습니다. ");
    printf("(%d-%d-%d %d:%d)\n", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min);
    
    // 클라이언트 연결 종료 시 처리
    pthread_mutex_lock(&mutx);
    for (i=0; i<clnt_cnt; i++)
    {
        if (clnt_sock == clnt_socks[i].sock)
        {
            for(; i<clnt_cnt-1; i++)
                clnt_socks[i] = clnt_socks[i+1]; // 클라이언트 배열에서 제거
            clnt_cnt--; //클라이언트 수 감소
            break;
        }
    }
    printf(" chatter (%d/100)\n", clnt_cnt);
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
    free(clnt); //메모리 해제
    return NULL;
}

// send_msg 함수: 모든 클라이언트에게 메시지 전송
void send_msg(char* msg, int len)
{
    int i;
    pthread_mutex_lock(&mutx);
    for (i=0; i<clnt_cnt; i++)
        write(clnt_socks[i].sock, msg, len);
    pthread_mutex_unlock(&mutx);
}

// error_handling 함수: 에러 메시지 출력 및 프로그램 종료
void error_handling(char *msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

// serverState 함수: 서버 상태 문자열 반환
char* serverState(int count)
{
    char* stateMsg = malloc(sizeof(char) * 20);
    strcpy(stateMsg ,"None");
    
    if (count < 5)
        strcpy(stateMsg, "Good");
    else
        strcpy(stateMsg, "Bad");
    
    return stateMsg;
}        

// menu 함수: 서버 메뉴 및 상태 정보 출력
void menu(char port[])
{
    system("clear");
    printf(" <<<< Chat server >>>>\n");
    printf(" Server port    : %s\n", port);
    printf(" Server state   : %s\n", serverState(clnt_cnt));
    printf(" Max Client : %d\n", MAX_CLNT);
    printf("\n <<<<          Log         >>>>\n\n");
}

// send_client_names 함수: 클라이언트 이름 목록 전송
void send_client_names(int clnt_sock) {
    int i;
    int total_length = 0; // 모든 클라이언트 이름의 총 길이를 저장할 변수
    char* msg;
    const char* header = "\n<<현재 접속한 클라이언트 목록>>\n";
    const char* format = "- %s\n";

    pthread_mutex_lock(&mutx); // 동시 접근 방지를 위한 뮤텍스 잠금

    // 메시지의 총 길이를 계산
    total_length += strlen(header);
    for(i = 0; i < clnt_cnt; i++) {
        total_length += strlen(clnt_socks[i].name) + strlen(format) - 2; // 이름 길이 + 개행 문자
    }

    // 메시지를 저장할 충분한 크기의 버퍼를 할당합니다.
    msg = (char*)malloc(total_length + 1); // +1은 문자열 종료를 위한 NULL 문자
    strcpy(msg, header); // 헤더 복사

    // 클라이언트 이름을 메시지에 추가
    for(i = 0; i < clnt_cnt; i++) {
        // 클라이언트 이름의 끝을 찾아서 NULL 문자로 종료
        char* end_of_name = strrchr(clnt_socks[i].name, ']');
        if (end_of_name != NULL) {
            *(end_of_name + 1) = '\0'; // ']' 문자 다음 위치를 NULL 문자로 대체
        }
        sprintf(msg + strlen(msg), format, clnt_socks[i].name);
    }

    pthread_mutex_unlock(&mutx); // 뮤텍스 잠금 해제

    // 요청한 클라이언트에게만 메시지 전송
    write(clnt_sock, msg, strlen(msg));

    free(msg); // 동적 할당된 메모리 해제
}