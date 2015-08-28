#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#define PORT 10080
#define MAX_MEMBER 10

void doExitTask(int fd, int fdArray[], int fdc, fd_set* readfdsFactory, int legalExit);
void spread(char param[], int fdc, int fd, int fdArray[]);
void sendInitMessage(int fd);
void sendCapacityError(int fd);
void notifyNewMember(int newfd, int fdArray[], int fdc);
void notifyMemberExit(int fd, int fdArray[], int fdc);
int  isExit(char* param);
int  getEmptyArrayOffset(int fdc, int fdArray[]);

int main(){
  struct sockaddr_in sockAddr;
  fd_set readfds, readfdsFactory;
  char buf[2048];
  int i, j, k, s, eoffset, newfd, isMaxCap, receivedDataSize, sockAddrSize, maxfd, fdc = 0;
  int fdArray[MAX_MEMBER];

  maxfd = s = socket(AF_INET, SOCK_STREAM, 0);
  perror("make socket");

  /* sockAddr 構造体などの設定 */
  bzero((char*) &sockAddr, sizeof(sockAddr));
  memset((char*) &sockAddr, 0, sizeof(sockAddr));
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
    /* maxfd計算 */
    for (i = 0; i < fdc; i++)
      if (fdArray[i] > maxfd) maxfd = fdArray[i];

    if (maxfd == -1) maxfd = s;
    memcpy(&readfds, &readfdsFactory, sizeof(fd_set));
    FD_SET(s, &readfds);
    /* 集合のfdが読み込み可能になるまで待つ */
    select(maxfd+1, &readfds, NULL, NULL, NULL);
    perror("select");

    /* トークメンバをaccept()できたら集合に加える処理。*/
    if (FD_ISSET(s, &readfds)) {
      FD_CLR(s, &readfds);

      newfd = accept(s, (struct sockaddr*) &sockAddr, &sockAddrSize);
      perror("accept");

      eoffset = getEmptyArrayOffset(fdc, fdArray);
      if (fdc >= MAX_MEMBER && eoffset == -1) {
        sendCapacityError(newfd);
        isMaxCap = 1;
      }

      if (newfd > 0 && isMaxCap != 1) {
        if (eoffset == -1) {
          fdArray[fdc] = newfd;
          fdc++;
        } else {
          fdArray[eoffset] = newfd;
        }	 
        FD_SET(newfd, &readfdsFactory);
        sendInitMessage(newfd);
        notifyNewMember(newfd, fdArray, fdc-1);
      }
    }

    /* メッセージを受信・送信する処理 */
    for (j = 0; j < fdc; j++) {
      if (FD_ISSET(fdArray[j], &readfds)) {
        memset(buf, '\0', sizeof(buf));
        receivedDataSize = recv(fdArray[j], buf, sizeof(buf), 0);
        perror("recv");
        if(receivedDataSize > 0) {
          if(isExit(buf)) {
            doExitTask(fdArray[j], fdArray, fdc, &readfdsFactory, 1);
            isMaxCap = 0;
            maxfd = -1;
          } else spread(buf, fdc, fdArray[j], fdArray);
        } else if (receivedDataSize == 0) {
          doExitTask(fdArray[j], fdArray, fdc, &readfdsFactory, 0);
        }
      }
    }
  
  }
  return 0;
}


int isExit(char* message){
  int co = 0;
  while(message[co] != '\0') co++;
  printf("%d\n", co);
  if (message[0] == '%' && message[1] == 'Q' && co == 3) return 1;
  else return 0;
}

void spread(char param[], int fdc, int fd, int fdArray[]) {
  int i, co = 0, co_self = 0;
  char sendable[2048];
  char sendable_self[2048];

  sprintf(sendable, "ユーザー%d: ", fd);
  sprintf(sendable_self, "自分: ");
  strcat(sendable, param);
  strcat(sendable_self, param);

  while(sendable[co] != '\0') co ++;
  while(sendable[co_self] != '\0') co_self ++;

  for (i = 0; i < fdc; i++) {
    if (fdArray[i] > -1) {
      if(fdArray[i] == fd) send(fdArray[i], sendable_self, co_self, 0);
      else send(fdArray[i], sendable, co, 0);
      perror("spread");
    }
  }
}

oid doExitTask(int fd, int fdArray[], int fdc, fd_set* pReadfdsFactory, int legalExit) {
 int i, co = 0;

 if (legalExit) {
   char sendable[100];
   sprintf(sendable, "******** Goodbye.\n");
   while(sendable[co] != '\0') co++;
   send(fd, sendable, co, 0);
   perror("doExitTask");
 }

  close(fd);
  for (i = 0; i < fdc; i++)
    if (fdArray[i] == fd) fdArray[i] = -1;
  FD_CLR(fd, pReadfdsFactory);
  notifyMemberExit(fd, fdArray, fdc);
}

void sendInitMessage(int fd) {
  char sendable[100];
  int i, co = 0;
  sprintf(sendable, "\n******** ユーザー%dとして参加しました。%%Qで退出できます.\n", fd);
  while(sendable[co] != '\0') co++;
  send(fd, sendable, co, 0);
  perror("sendInitMessage");
}

void notifyNewMember(int newfd, int fdArray[], int fdc) {
  int i, co = 0;
  char sendable[100];
  sprintf(sendable, "******** ユーザー%d が参加してきました。\n", newfd);
  while(sendable[co] != '\0') co ++;
  for (i = 0; i < fdc; i++) {
    if (i != newfd && fdArray[i] > -1) {
      send(fdArray[i], sendable, co, 0);
      perror("notifyNewMember");
    }
  }
}

void notifyMemberExit(int fd, int fdArray[], int fdc) {
  int i, co = 0;
  char sendable[100];
  sprintf(sendable, "******** ユーザー %d が退出しました。\n", fd);
  while(sendable[co] != '\0') co++;
  for (i = 0; i < fdc; i++) {
    if (fdArray[i] > -1) {
      send(fdArray[i], sendable, co, 0);
      perror("notifyMemberExit");
    }
  }
}

void sendCapacityError(int fd) {
  int co = 0;
  char sendable[100];
  sprintf(sendable, "\n******** トークルームは満員です。参加できません。\n");
  while(sendable[co] != '\0') co++;
  send(fd, sendable, co, 0);
  perror("sendCapacityError");
  close(fd);
}

int getEmptyArrayOffset(int fdc, int fdArray[]) {
   int i;
   for (i = 0; i < fdc; i++) {
     if (fdArray[i] == -1) return i;
   }
   return -1;
}

