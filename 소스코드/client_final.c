/**    chat_client **/
 
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<pthread.h>
#include<time.h>

// 단위는 Byte
#define BUF_SIZE 200 //메세지 버퍼의 크키
#define NORMAL_SIZE 50 //새로운 이름을 넣기 위한 버퍼

// 함수 프로토타입 선언
void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);
void menu();

char name[NORMAL_SIZE]="[DEFALT]";  // 사용자 이름 저장
char msg_form[NORMAL_SIZE];         // 메시지 형식 저장
char serv_time[NORMAL_SIZE];        // 서버 시간 저장
char msg[BUF_SIZE];                 // 메시지 내용 저장
char serv_port[NORMAL_SIZE];        // 서버 포트 번호 저장
char clnt_ip[NORMAL_SIZE];          // 클라이언트 IP 주소 저장
char prev_name[NORMAL_SIZE];        // 이전 사용자 이름 저장

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    // 송신 쓰레드와 수신 쓰레드로 총 2개의 쓰레드 선언
    // 내 메세지를 보내야하고, 상대방의 메세지도 받아야 한다.
    pthread_t snd_thread, rcv_thread;
    void* thread_return;

    if (argc!=4)
    {
        printf(" Usage : %s <ip> <port> <user_name>\n", argv[0]);
        exit(1);
    }
 
    /** local time **/
    struct tm *t;
    time_t timer = time(NULL);
    t=localtime(&timer);
    sprintf(serv_time, "%d-%d-%d %d:%d", t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min);

    // argv[3] 이 Jony 라면, "[Jony]" 가 name 이 됨
    sprintf(name, "[%s]", argv[3]); // 사용자 이름 설정
    sprintf(serv_port, "%s", argv[2]); // 서버 포트 번호 설정
    sprintf(clnt_ip, "%s", argv[1]); // 클라이언트 IP 주소 설정
    sock=socket(PF_INET, SOCK_STREAM, 0); // 소켓 생성
    
    // 서버 주소 구조체 설정
    memset(&serv_addr, 0, sizeof(serv_addr)); // serv_addr 구조체의 모든 메모리를 0으로 초기화
    serv_addr.sin_family=AF_INET; // 주소 체계를 IPv4 인터넷 프로토콜로 설정
    serv_addr.sin_addr.s_addr=inet_addr(argv[1]); // IP 주소 설정
    serv_addr.sin_port=htons(atoi(argv[2])); // 포트 번호 설정
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
        error_handling(" conncet() error"); // 연결 에러 시 에러 핸들링 함수 호출
 
    menu(); // 메뉴 호출

    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock); // 송신 쓰레드 생성
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock); // 수신 쓰레드 생성
    pthread_join(snd_thread, &thread_return); // 송신 쓰레드가 종료될 때까지 대기
    pthread_join(rcv_thread, &thread_return); // 수신 쓰레드가 종료될 때까지 대기
    close(sock); // 소켓 닫음
    return 0;
}
 
void* send_msg(void* arg)
{
    int sock=*((int*)arg); //server sock을 의미, 서버 소켓 파일 디스크립터 가져옴
    
    // 사용자 아이디와 메세지를 "붙여서" 한 번에 보낼 것이다
    char name_msg[NORMAL_SIZE+BUF_SIZE]; //NORMAL_SIZE : 20 / BUF_SIZE :100
    char myInfo[BUF_SIZE]; // 입장 및 퇴장 메시지 저장 버퍼
    char my_name[BUF_SIZE]; // 사용자 이름 저장 버퍼
    
    sprintf(my_name,"%s",name); // my_name 버퍼에 포맷팅, 사용자 이름을 my_name 버퍼에 복사
    write(sock, my_name, strlen(my_name)); // 사용자 이름을 서버에 전송

    /** send join messge **/
    printf("%s님 환영합니다!\n",name); // 콘솔에 출력
    sprintf(myInfo, "⭐%s님이 입장하셨습니다.⭐\n",name); // 입장 메시지를 myInfo 버퍼에 저장
    write(sock, myInfo, strlen(myInfo)); // 소켓으로 메시지를 서버에 전송

    while(1)
    {   
        fgets(msg, BUF_SIZE, stdin); // 사용자로부터 메시지를 입력 받음

        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) // 'q' 또는 'Q'를 입력받으면 종료
        {
            // send leave message 생성
            sprintf(myInfo, "%s님이 채팅방을 나갔습니다!!\n", name); // 퇴장 메시지를 myInfo 버퍼에 저장
            // send leave message 출력
            write(sock, myInfo, strlen(myInfo)); // 퇴장 메시지를 서버에 전송

            close(sock); // 소켓 닫음
            exit(0); // 프로그램 종료
        }
        
        else if(!strcmp(msg, "0\n")){ // '0'을 입력받으면 모드 선택 메뉴 출력
            printf(" <<<<    Select mode    >>>>\n 1. Chatting Clear\n 2. Change name\n 3. Connected Client List\n The ohter key is cancel");
            printf("\n ================================\n");
            printf(" Select mode >> ");
            fgets(msg, BUF_SIZE, stdin); // 사용자로부터 모드 선택을 다시 받음

                if(!strcmp(msg, "1\n")){ // '1'을 입력받으면 채팅 화면을 지움
                    menu();
                    printf(" Rejoin the chat.\n");
                    continue;
                }
                else if(!strcmp(msg,"2\n")){ // '2'를 입력받으면 이름 변경 모드로 진입
                    // 현재이름(name)을 이전 이름(prev_name) 잠시 저장
                    strncpy(prev_name, name, NORMAL_SIZE); 
                    printf("\n바꿀 이름을 입력하세요 -> ");
                    fgets(name, NORMAL_SIZE, stdin); // 새로운 이름을 입력받음
                    name[strcspn(name, "\n")] = 0; // 줄바꿈 문자 제거
                    
                    // 대괄호를 포함하여 name 변수 업데이트
                    char temp_name[BUF_SIZE]; // 새로운 이름을 임시 저장할 버퍼 선언
                    sprintf(temp_name, "[%s]", name); // 대괄호 추가
                    strncpy(name, temp_name, NORMAL_SIZE); // name 변수로 다시 복사 후 업데이트

                    // 이름 변경 요청 메시지
                    char name_change_request[BUF_SIZE]; // 이름 변경 요청 메시지를 저장할 버퍼 선언
                    sprintf(name_change_request, "NAME_CHANGE %s", name); // 서버에 보낼 이름 변경 요청 메세지 생성
                    write(sock, name_change_request, strlen(name_change_request)); // 생성된 메시지를 서버로 전송

                    // 이름 변경 알림 메시지
                    sprintf(name_msg,"사용자 이름 %s 에서 %s 로 변경되었습니다.\n", prev_name, name);
                    write(sock, name_msg, strlen(name_msg));
                    continue;
                }
                else if(!strcmp(msg,"3\n")){
                     // 참가자 명단 출력
                    char req_msg[] = "LIST_CLIENTS\n"; // 클라이언트 목록 요청 메시지 생성
                    write(sock, req_msg, strlen(req_msg)); // 클라이언트 목록 요청을 서버에 전송
                }
                else{
                    printf(" Rejoin the chat.\n");
                    continue;
                }
            continue;
        }
        // send message
        // name 를 "Jony", msg 를 "안녕 얘들아" 로 했다면, => [Jony] 안녕 얘들아
        // 이것이 name_msg 로 들어가서 출력됨
        sprintf(name_msg,"%s %s", name, msg); //이름이랑 내용이 결합되어 출력
            
        // 서버로 메세지 보냄
        write(sock, name_msg, strlen(name_msg));
    }
    return NULL;
}

// recv_msg 함수: 다른 클라이언트로부터 메시지를 수신하는 함수
// 다른 클라이언트가 입력한 메시지를 받아오는 코드
void* recv_msg(void* arg)
{
    int sock=*((int*)arg); //server sock을 의미, 서버 소켓 파일 디스크립터 가져옴
    char name_msg[NORMAL_SIZE+BUF_SIZE]; // 수신된 메시지 저장할 버퍼
    int str_len; // read 함수로부터 반환된 문자열의 길이 저장
    
    while(1)
    {
        str_len=read(sock, name_msg, NORMAL_SIZE+BUF_SIZE-1); // 서버로부터 메시지를 읽어옴
        if (str_len==-1)
            return (void*)-1; // read 함수가 -1을 반환하면 오류가 발생한 것이므로 함수 종료
        name_msg[str_len]=0; // 문자열의 끝을 나타내는 Null 문자 추가
        fputs(name_msg, stdout); // 받은 메세지 출력
    }
    return NULL; // 함수가 정상적으로 종료되면 Null 반환
}

void menu()
{
    system("clear");
    printf(" <<<< Chat Client >>>>\n");
    printf(" Server Port : %s \n", serv_port);
    printf(" Client IP   : %s \n", clnt_ip);
    printf(" Chat Name   : %s \n", name);
    printf(" Server Time : %s \n", serv_time);
    printf(" ============= Mode =============\n");
    printf(" 0. Select mode\n"); // 이름 변경과 메뉴로 복귀
    printf("  --> 1. Chatting Clear\n");
    printf("  --> 2. Change name\n");
    printf("  --> 3. Connected Client List\n");
    printf(" ================================\n");
    printf(" Exit -> q & Q\n\n");
}    

// error_handling 함수: 에러 메시지를 처리하고 프로그램을 종료하는 함수
void error_handling(char* msg)
{
    fputs(msg, stderr); // 표준 에러(stderr)에 에러 메시지 출력
    fputc('\n', stderr); // 표준 에러에 개행 문자 추가
    exit(1); // 에러가 발생했으므로 프로그램 종료
}