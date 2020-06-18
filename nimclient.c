#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"

int receiveAndSend(int sock);
void printPiles(int sock, int pileNumber);

int main()
{
  printf("Connecting...\n");
  int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("socket()");
    return 1;
  }

  //set socket address
  struct sockaddr_un addr = {
    .sun_family = AF_LOCAL,
    .sun_path = SOCKADDR
  };

  if(connect(sock, (struct sockaddr *)&addr, sizeof addr) == -1){
    fprintf(stderr, "Impossible to connect to the server.\n");
    return 2;
  }
  int buflen = 0;
  recv(sock, &buflen, sizeof(buflen), 0);
  char *buffer = malloc(buflen);
  recv(sock, buffer, buflen, 0); //receive connection msg
  printf("%s\n", buffer);
  recv(sock, &buflen, sizeof(buflen), 0);
  buffer = realloc(buffer, buflen);
  recv(sock, buffer, buflen, 0); //receive welcome msg
  printf("%s\n", buffer);

  char input[40];

  int playerID;
  int pileNumber;
  int validationSignal;

  //receives player ID
  recv(sock, &playerID, sizeof(playerID), 0);
  //Sets the number of piles if player 1
  if (playerID == 1){
    printf("Please insert the number of piles to play with. (More than 1)\n");
    while(1){
      fgets(input, 40, stdin);
      sscanf(input, "%d", &pileNumber);
      send(sock, &pileNumber, sizeof(pileNumber), 0);
      recv(sock, &validationSignal, sizeof(validationSignal), 0);
      if (validationSignal == 83){ // Check for validation on number of piles chosen
        break;
      } else if (validationSignal == 84){
        recv(sock, &buflen, sizeof(buflen), 0);
        char *buffer = malloc(buflen);
        recv(sock, buffer, buflen, 0); //receive msg
        printf("%s\n", buffer);
      } else {
        fprintf(stderr, "Error 01. Invalid signal or client has lost connection.\n");
        return 3;
      }
    }
  } else if (playerID == 2){
    recv(sock, &pileNumber, sizeof(pileNumber), 0);
  }
  int pile, turnSignal, bufSignal;
  int playSignal = 1;
  int waitSignal = 0;
  int endSignal = 90;
  int check;
  
  printPiles(sock, pileNumber);

  while(1){
    recv(sock, &bufSignal, sizeof(bufSignal), 0);
    if (bufSignal == playSignal){      
      check = receiveAndSend(sock); //choose pile
      if (check == 1){
        fprintf(stderr, "Error 02. Invalid signal or client has lost connection.\n");
        return 4;
      }
      check = receiveAndSend(sock); //choose elements
      if (check == 1){
        fprintf(stderr, "Error 03. Invalid signal or client has lost connection.\n");
        return 5;
      }
      //Print piles
      printPiles(sock, pileNumber);
    } else if (bufSignal == waitSignal){
      //receive waiting messagge
      recv(sock, &buflen, sizeof(buflen), 0);
      buffer = realloc(buffer, buflen);
      recv(sock, buffer, buflen, 0);
      printPiles(sock, pileNumber);
    } else {
      fprintf(stderr, "Error 04. Invalid signal or client has lost connection.\n");
      return 6;
    }
    //Receive signal
    recv(sock, &bufSignal, sizeof(bufSignal), 0);
    if(bufSignal == endSignal){ //if end then break while cycle
      recv(sock, &buflen, sizeof(buflen), 0);
      buffer = realloc(buffer, buflen);
      recv(sock, buffer, buflen, 0); //receive winning msg
      printf("%s\n", buffer);
      break;
    }
  }

  printf("Closing client...\n");
  return 0;
}
/*
* Receive a string message and send an integer via a UNIX socket
* @param sock, the socket to use
*/
int receiveAndSend(int sock){
  int bufInt, buflen;
  char input[40];

  int validationSignal;

  recv(sock, &buflen, sizeof(buflen), 0);
  char *buffer = malloc(buflen);
  recv(sock, buffer, buflen, 0); //receive turn msg
  printf("%s\n", buffer);
  while(1){
    fgets(input, 40, stdin);
    sscanf(input, "%d", &bufInt);
    send(sock, &bufInt, sizeof(bufInt), 0);
    recv(sock, &validationSignal, sizeof(validationSignal), 0);
    if (validationSignal == 85){ // Check for validation on number of piles chosen
        break;
      } else if (validationSignal == 86){
        recv(sock, &buflen, sizeof(buflen), 0);
        char *buffer = malloc(buflen);
        recv(sock, buffer, buflen, 0); //receive msg
        printf("%s\n", buffer);
      } else {
        return 1;
      }
  }
  return 0;
}
/*
* Prints the content, after receiving it via a UNIX socket, of all the piles currently in the game
* @param sock, the socket to use
* @param pileNumber the number of piles in the game
*/
void printPiles(int sock, int pileNumber){
  int pile, buflen;
  printf("Current piles are: \n");
  for (int i = 0; i < pileNumber; i++){
    recv(sock, &pile, sizeof(pile), 0);
    printf("Pile %d: %d\n", i, pile);
  }
  printf("\n");
}