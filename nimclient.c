#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"

int receiveAndSend(int sock);
int printPiles(int sock, int pileNumber);
int receiveMsg(int sock);

int main()
{
  printf("Connecting...\n");
  int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
  if(sock == -1) {
    fprintf(stderr, "Error 01. Socket error");
    return 1;
  }

  //set socket address
  struct sockaddr_un addr = {
    .sun_family = AF_LOCAL,
    .sun_path = SOCKADDR
  };


  if(connect(sock, (struct sockaddr *)&addr, sizeof addr) == -1){
    fprintf(stderr, "Error 02. Impossible to connect to the server.\n");
    return 2;
  }

  int test;

  int buflen = 0;
  char *buffer = malloc(buflen);

  struct pollfd fd;
  fd.events = POLLIN;
  fd.fd = sock;

  for (int i = 0; i < 2; i++){
    test = receiveMsg(sock);
    if (test == -1){
      fprintf(stderr, "Error 03. No message received.\n");
      return 3;
    }
  }

  char input[40];

  int playerID;
  int pileNumber;
  int validationSignal;
  int scancheck;

  //receives player ID
  test = recv(sock, &playerID, sizeof(playerID), 0);
  if (test == -1) {
    fprintf(stderr, "Error 03. No message received.\n");
    return 3;
  }
  //Sets the number of piles if player 1
  if (playerID == 1){
    while(1){
      while(scancheck != 1){ //Check that input is a single integer
        printf("Please insert the number of piles to play with. (More than 1)\n");
        fgets(input, 40, stdin);
        scancheck = sscanf(input, "%d ", &pileNumber);
      }
      test = send(sock, &pileNumber, sizeof(pileNumber), MSG_NOSIGNAL);
      if (test == -1){
        fprintf(stderr, "Error 04. Couldn't send message.\n");
        return 4;
      }
      test = recv(sock, &validationSignal, sizeof(validationSignal), 0);
      if (test == -1) {
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      }

      if (validationSignal == 83){ // Check for validation on number of piles chosen
        break;
      } else if (validationSignal == 84){ // If the sent message isn't valid, resend it
        test = receiveMsg(sock);
        if (test == -1){
          fprintf(stderr, "Error 03. No message received.\n");
          return 3;
        }
      } else { //Print error if the signal isn't one of the valid options
        fprintf(stderr, "Error 05. Invalid signal.\n");
        return 5;
      }
    }
  } else if (playerID == 2){
    test = poll(&fd, 1, 60000); // 60 second for timeout
    switch (test) {
        case -1:
            fprintf(stderr, "Error 03. No message received.\n");// Error
            return 3;;
        case 0:
            fprintf(stderr, "Error 07. Timeout.\n");// Timeout 
            return 7;;
        default:
            recv(sock, &pileNumber, sizeof(pileNumber), 0);
            break;
    }
  }
  int pile, turnSignal, bufSignal;
  int playSignal = 1;
  int waitSignal = 0;
  int endSignal = 90;
  test = printPiles(sock, pileNumber);
  if (test == -1){
    fprintf(stderr, "Error 03. No message received.\n");
    return 3;
  } else if (test == -2){
    fprintf(stderr, "Error 05. Invalid signal.\n");
    return 5;
  }

  while(1){
    test = recv(sock, &bufSignal, sizeof(bufSignal), 0);
    if (test == -1 ){
      fprintf(stderr, "Error 03. No message received.\n");
      return 3;
    }
    if (bufSignal == playSignal){ // Playing turn
      test = receiveAndSend(sock); //choose pile
      if (test != 0){
        fprintf(stderr, "Error 08. Player turn error.\n");
        return 8;
      }
      test = receiveAndSend(sock); //choose elements
      if (test != 0){
        fprintf(stderr, "Error 08. Player turn error.\n");
        return 8;
      }
      //Print piles
      test = printPiles(sock, pileNumber);
      if (test == -1){
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      } else if (test == -2){
        fprintf(stderr, "Error 05. Invalid signal.\n");
        return 5;
      }
    } else if (bufSignal == waitSignal){ //Waiting turn
      //receive waiting messagge
      test = receiveMsg(sock);
      if (test == -1){
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      }
      test = printPiles(sock, pileNumber);
      if (test == -1){
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      } else if (test == -2){
        fprintf(stderr, "Error 05. Invalid signal.\n");
        return 5;
      }
    } else { //Error
      fprintf(stderr, "Error 05. Invalid signal.\n");
      return 5;
    }
    //Receive signal
    test = recv(sock, &bufSignal, sizeof(bufSignal), 0);
    if (test == -1 ){
      fprintf(stderr, "Error 03. No message received.\n");
      return 3;
    }
    if(bufSignal == endSignal){ //if end then break while cycle
      test = receiveMsg(sock);
      if (test == -1){
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      }
      break;
    }
  }

  printf("Closing client...\n");
  return 0;
}
/*
* Receive a string message and send an integer via a UNIX socket
* @param sock, the socket to use
* @param 0 if no issues have been encountered
*/
int receiveAndSend(int sock){
  int bufInt, buflen;
  char input[40];
  int test;

  int validationSignal;

  test = receiveMsg(sock);
  if (test == -1){
    return 2;
  }
  int scancheck;
  while(1){
    while(scancheck != 1){ //Check that input is a single integer
      fgets(input, 40, stdin);
      scancheck = sscanf(input, "%d ", &bufInt);
    }
    scancheck = 0;
    test = send(sock, &bufInt, sizeof(bufInt), MSG_NOSIGNAL);
    if (test == -1){
      fprintf(stderr, "Error 04. Couldn't send message.\n");
      return 4;
    }
    test = recv(sock, &validationSignal, sizeof(validationSignal), 0);
    if (test == -1 ){
      return -1;
    }
    if (validationSignal == 85){ // Check for validation on number of piles chosen
        break;
    } else if (validationSignal == 86){
      test = receiveMsg(sock);
      if (test == -1){
        fprintf(stderr, "Error 03. No message received.\n");
        return 3;
      }
    } else {
      fprintf(stderr, "Error 05. Invalid signal.\n");
      return 5;
    }
  }
  return 0;
}
/*
* Prints the content, after receiving it via a UNIX socket, of all the piles currently in the game
* @param sock, the socket to use
* @param pileNumber the number of piles in the game
* @return 0 if no issues have been encountered, otherwise -1 or -2
*/
int printPiles(int sock, int pileNumber){
  int buffer, test;
  int validSignal = 92;
  test = recv(sock, &buffer, sizeof(buffer), 0);
  if (test == -1 ){
    return -1;
  }
  if (buffer != validSignal){
    return -2;
  }

  printf("Current piles are: \n");
  for (int i = 0; i < pileNumber; i++){
    test = recv(sock, &buffer, sizeof(buffer), 0);
    if (test == -1 || test == 0 ){
      return -1;
    }
    printf("Pile %d: %d\n", i, buffer);
  }
  printf("\n");
  return 0;
}

/*
* Receive message from a socket
* @param sock, the socket to use
* @return 0 or -1 if any errors have been encountered
*/
int receiveMsg(int sock){
  int buflen = 0;
  char *buffer = malloc(buflen);
  int test;

  test = recv(sock, &buflen, sizeof(buflen), 0);
  if (test == -1 ){
    return -1;
  }
  test = recv(sock, buffer, buflen, 0); //receive connection msg
  if (test == -1 ){ // Print error if message wasn't received
    return -1;
  }
  printf("%s\n", buffer);
  return 0;
}
