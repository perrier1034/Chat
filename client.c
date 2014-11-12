#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_BUF_LENGTH 100
#define MAX_REQ_LENGTH 100
#define REQ_GET_HTML "GET / HTTP/1.0\r\n\r\n"

/*
 * プロセス内で関数を呼ぶと変な挙動になるので全部 main() に収めた。
 */
 int main(int argc, char* argv[]) {

   struct hostent* host;
   struct sockaddr_in sockAddr;
   int port, isExit, s, pid, co, receivedDataSize, sockAddrSize;
   char buf[MAX_BUF_LENGTH] = {};
   char req[MAX_REQ_LENGTH] = {};

   if (argc != 3)
     printf("Invalid arguments: Only ./client localhost PORT  is acceptable.\n");

   port = atoi(argv[2]);

   if((host = gethostbyname(argv[1])) < 0)
     perror("get host");

   if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      perror("make socket");

   /* sockAddr 構造体などの設定 */
   bzero((char *) &sockAddr, sizeof(sockAddr));
   bcopy((char *)host -> h_addr, (char *) &sockAddr.sin_addr, host -> h_length);
   sockAddr.sin_family = AF_INET;
   sockAddr.sin_port = htons(port);
   sockAddrSize = sizeof(sockAddr);

   if(connect(s, (struct sockaddr *)&sockAddr, sockAddrSize) < 0)
     perror("connect");

   /* InitialMessageを受信 */
   if((receivedDataSize = recv(s, buf, sizeof(buf), 0) < 0))
     perror("recv");

   /* InitialMessageを表示 */
   printf("%s\n", buf);
   memset(buf, '\0', sizeof(buf));

   pid = fork();
   if (pid > 0) { /* 親プロセスの動作 */
     while (1) {
       memset(buf, '\0', sizeof(buf));
       receivedDataSize = recv(s, buf, sizeof(buf), 0);
       if (receivedDataSize > 0) {
	 printf("%s\n", buf);
       } else if (receivedDataSize == 0) {
	 if (close(s) < 0) perror("close socket");
	 break;
       } else {
	 perror("receive");
	 break;
       }
     }
   } else if (pid == 0) { /* 子プロセス生成に成功、その動作 */
     while (1) {
       co = 0;
       fgets(req, sizeof(req), stdin);
       while(req[co] != '\0') co++;
       if(send(s, req, co, 0) < 0)
	 perror("send");
     } 
   } else /* childプロセス生成失敗 */
     perror("fork");

   return 0;
 }
