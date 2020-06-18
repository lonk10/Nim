#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>


#include "common.h"

#define RANMAX 50;

void playerTurn(int playID, int waitID, int* piles, int pileNumber);
int checkPilesContent(int* piles, int pileNumber);
void sendPiles(int fd1, int fd2, int* piles, int pileNumber);
void handle_sigchld(int);
void sendMsg(int fd, char* msg);

int main (int argc, char **argv)
{
  // signal handler
  signal(SIGCHLD, handle_sigchld);

  // open socket
  int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("socket()");
    return 1;
  }

  // set socket address
  struct sockaddr_un addr = {
    .sun_family = AF_LOCAL,
    .sun_path = SOCKADDR
  };

  // unlink socket
  unlink(SOCKADDR);
  // bind addr to sock
  if(bind(sock, (struct sockaddr *)&addr, sizeof addr) == -1) {
    perror("bind()");
    return 2;
  }

  // start listening
  listen(sock, 4);
  fprintf(stderr, "Listening.\n");

  // listen loop
  while (1)
  {
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    int fd1 = accept(sock, (struct sockaddr *)&client_addr, &client_len);
    if(fd1 == -1) {
      perror("accept()");
      return 3;
    }
    char waitmsg1[] = "Connected. You are Player 1. Waiting for Player 2...\n";
    sendMsg(fd1, waitmsg1);

    struct sockaddr_un client2_addr;
    socklen_t client2_len = sizeof(client2_addr);
    int fd2 = accept(sock, (struct sockaddr *)&client2_addr, &client2_len);
    if(fd2 == -1) {
      perror("accept()");
      return 3;
    }
    char waitmsg2[] = "Connected. You are Player 2, please wait for Player 1 to choose the number of piles.\n";
    sendMsg(fd2, waitmsg2);

    /*
     * forks when both clients are connected
     */
    pid_t pid = fork();
    if(pid == -1) {
      perror("fork()");
      return 4;
    }

    // child handles game
    if(!pid) {
        fprintf(stderr, "Opened connection (PID %d).\n",getpid());

        char greet[] = "Welcome to C_Nim!\n";
        int greetLen = strlen(greet) + 1;
        int pileNumber;
        int player1 = 1;
        int player2 = 2;

        sendMsg(fd1, greet); //Send greet to fd1
        sendMsg(fd2, greet); //Sent greet to fd2
        send(fd1, &player1, sizeof(player1), 0); // send player ID to fd1
        send(fd2, &player2, sizeof(player2), 0); // send player ID to fd2
        //Recieve from Player 1 the number of piles to play with and send it to Player 2
        recv(fd1, &pileNumber, sizeof(pileNumber), 0);
        send(fd2, &pileNumber, sizeof(pileNumber), 0);

        //Generate piles
        int* piles = malloc(pileNumber * sizeof(int));
        srandom(time(0));
        for (int i = 0; i < pileNumber; i++){
          piles[i] = random() % RANMAX; 
        }

        char player1WinMsg[] = "Player 1 has won. Closing game...\n";
        char player2WinMsg[] = "Player 2 has won. Closing game...\n";
        int contSignal = 89;
        int endSignal = 90;

        //Send piles to clients
        sendPiles(fd1, fd2, piles, pileNumber);
        while(1){
          //Player 1 turn
          playerTurn(fd1, fd2, piles, pileNumber);
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("PID %d. Player 1 has won, initiating ending sequence.\n", getpid());
            //send end signal
            send(fd1, &endSignal, sizeof(endSignal), 0);
            send(fd2, &endSignal, sizeof(endSignal), 0);
            //send win message
            sendMsg(fd1, player1WinMsg);
            sendMsg(fd2, player1WinMsg);
            break;
          } else {
            printf("Game is continuing.\n");
            //send cont signal
            send(fd1, &contSignal, sizeof(contSignal), 0);
            send(fd2, &contSignal, sizeof(contSignal), 0);
          }

          //Player 2 turn
          playerTurn(fd2, fd1, piles, pileNumber);
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("PID %d. Player 2 has won, initiating ending sequence.\n", getpid());
            //send end signal
            send(fd1, &endSignal, sizeof(endSignal), 0);
            send(fd2, &endSignal, sizeof(endSignal), 0);
            //send win message
            sendMsg(fd1, player2WinMsg);
            sendMsg(fd2, player2WinMsg);
            break;
          } else {
            printf("Game is continuing.\n");
            //send cont signal
            send(fd1, &contSignal, sizeof(contSignal), 0);
            send(fd2, &contSignal, sizeof(contSignal), 0);
          }
        }


        close(fd1);
        close(fd2); // Close connection with clients
        break;
    }
  }
}

void playerTurn(int playID, int waitID, int* piles, int pileNumber){
  char playerTurnPile[] = "Please choose a pile.\n";
  char playerTurnElements[] = "Please choose how many elements to remove.\n";
  char playerTurnWait[] = "Please wait for the other player's turn.\n";

  int chosenPile, chosenElements;

  int play = 1;
  int wait = 0;

  //Send play signal
  send(playID, &play, sizeof(play), 0);
  printf("Play signal sent.\n");
  //Send wait signal
  send(waitID, &wait, sizeof(wait), 0);
  printf("Wait signal sent.\n");

  //Send wait message to Player waiting
  sendMsg(waitID, playerTurnWait);
  printf("Wait message sent.\n");
  //Ask Player 1 which pile to take from
  sendMsg(playID, playerTurnPile);
  printf("Pile message sent.\n");
  recv(playID, &chosenPile, sizeof(chosenPile), 0);
  //Ask Player 1 how many elements to take
  sendMsg(playID, playerTurnElements);
  printf("Elements message sent.\n");
  recv(playID, &chosenElements, sizeof(chosenElements), 0);

  piles[chosenPile] -= chosenElements;
  //send piles to clients
  sendPiles(playID, waitID, piles, pileNumber);
  printf("Pile %d is now %d.\n", chosenPile, piles[chosenPile]);
}

int checkPilesContent(int* piles, int pileNumber){
  int result = 0;
  //check piles for a non-zero pile
  for (int i = 0; i < pileNumber; i++){
    if (piles[i] > 0){
      result = 1; //result changed to 1 if such pile is found
    }
  }
  printf("Res: %d\n", result);
  return result;
}

void handle_sigchld(int x) {
    int saved_errno = errno;
    while (waitpid(-1, 0, WNOHANG) > 0) {}
    errno = saved_errno;
}

void sendPiles(int fd1, int fd2, int* piles, int pileNumber){
  //Send piles to clients
  for (int i = 0; i < pileNumber; i++){
    send(fd1, &piles[i], sizeof(piles[i]), 0);
    send(fd2, &piles[i], sizeof(piles[i]), 0);
  }
}

void sendMsg(int fd, char* msg){
  int msgLen = strlen(msg) + 1;
  send(fd, &msgLen, sizeof(msgLen), 0); // Send msg length
  send(fd, msg, msgLen, 0); // Send msg
}