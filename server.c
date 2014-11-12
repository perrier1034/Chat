#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#define PORT 10084

 void doExitTask(int fd, int fdArray[], int fdc, fd_set* readfdsFactory);
 void spread(char param[], int fdc, int fd, int fdArray[]);
 void sendInitMessage(int fd);
 void notifyNewMember(int newfd, int fdArray[], int fdc);
 void notifyExit(int fd, int fdArray[], int fdc);
 int  isExit(char* param);

 int main(){
   struct sockaddr_in sockAddr;
   fd_set readfds, readfdsFactory;
   char buf[2048];
   int i, j, s, receivedDataSize, sockAddrSize, maxfd, fdc = 0;
   int fdArray[100];

   maxfd = s = socket(AF_INET, SOCK_STREAM, 0);
   perror("make socket");

   /* sockAddr 構造体などの設定 */
   bzero((char *) &sockAddr, sizeof(sockAddr));
   memset((char *) &sockAddr, 0, sizeof(sockAddr));
   sockAddr.sin_family = AF_INET;
   sockAddr.sin_addr.s_addr = htonl(INADDR_ANY); 
   sockAddr.sin_port = htons(PORT);
   sockAddrSize = sizeof(sockAddr);
   
   bind(s, (struct sockaddr*) &sockAddr, sizeof(sockAddr));
   perror("bind");

   listen(s, 5);
   perror("listen");
  
   FD_ZERO(&readfdsFactory);

   /* 接続待ちfdを集合に加える */
   FD_SET(s, &readfdsFactory);
 
   while (1) {
     // maxfd計算
     for (i = 0; i < fdc; i++)
       if (fdArray[i] > maxfd) maxfd = fdArray[i];

     if (maxfd == -1) maxfd = s;
     // select()用に集合をコピーする。
     memcpy(&readfds, &readfdsFactory, sizeof(fd_set));
     
     FD_SET(s, &readfds);

     select(maxfd+1, &readfds, NULL, NULL, NULL);
     perror("select");

     // トークメンバをaccept()できたら集合に加える処理。
     if (FD_ISSET(s, &readfds)) {
       FD_CLR(s, &readfds);
       int newfd = accept(s, (struct sockaddr*) &sockAddr, &sockAddrSize);
       if (newfd > 0) {
	 fdArray[fdc] = newfd;
	 perror("accept");
	 FD_SET(newfd, &readfdsFactory);
	 sendInitMessage(newfd);
	 notifyNewMember(newfd, fdArray, fdc);
	 fdc++;
       }
     }

     // メッセージを送信する処理
     for (j = 0; j < fdc; j++) {
       if (FD_ISSET(fdArray[j], &readfds)) {
	 memset(buf, '\0', sizeof(buf));
	 receivedDataSize = recv(fdArray[j], buf, sizeof(buf), 0);
	 perror("recv");
	 if(receivedDataSize > 0) {
	   if(isExit(buf)) {
	     maxfd = -1;
	     doExitTask(fdArray[j], fdArray, fdc, &readfdsFactory);
	   } else spread(buf, fdc, fdArray[j], fdArray);
	 }
       }
     }
   
   }
   return 0;
 }


 int isExit(char* message){
   if (message[0] == '%' && message[1] == 'Q') return 1;
   else return 0;
 }

 void spread(char param[], int fdc, int fd, int fdArray[]) {
   int i, co = 0;
   char sendable[2048];
   sprintf(sendable, "USER-%d: ", fd);
   strcat(sendable, param);
   while(sendable[co] != '\0') co ++;

   for (i = 0; i < fdc; i++) {
     if (fdArray[i] > -1) {
       send(fdArray[i], sendable, co, 0);
       perror("send_spread");
     }
   }
 }

 void doExitTask(int fd, int fdArray[], int fdc, fd_set* pReadfdsFactory) {
   char sendable[100];
   int i, co = 0;
   sprintf(sendable, "*Goodbye.\n");
   while(sendable[co] != '\0') co++;
   send(fd, sendable, co, 0);
   perror("send1");
   FD_CLR(fd, pReadfdsFactory);
   close(fd);
   for (i = 0; i < fdc; i++)
     if (fdArray[i] == fd) fdArray[i] = -1;
   notifyExit(fd, fdArray, fdc);
 }

 void sendInitMessage(int fd) {
   char sendable[100];
   int i, co = 0;
   sprintf(sendable, "*Your are USER-%d. You can exit by typing %%Q.\n", fd);
   while(sendable[co] != '\0') co ++;
   send(fd, sendable, co, 0);
   perror("send_sendInitMessage");
 }

 void notifyNewMember(int newfd, int fdArray[], int fdc) {
   int i, co = 0;
   char sendable[100];
   sprintf(sendable, "*USER-%d joined.\n", newfd);
   while(sendable[co] != '\0') co ++;
   for (i = 0; i < fdc; i++) {
     if (i != newfd && fdArray[i] > -1) {
       send(fdArray[i], sendable, co, 0);
       perror("send_notifyNewMember");
     }
   }
 }

 void notifyExit(int fd, int fdArray[], int fdc) {
   int i, co = 0;
   char sendable[100];
   sprintf(sendable, "USER-%d exited.\n", fd);
   while(sendable[co] != '\0') co ++;
   for (i = 0; i < fdc; i++) {
     if (fdArray[i] > -1) {
       send(fdArray[i], sendable, co, 0);
       perror("send_notifyExit");
     }
   }
 }
