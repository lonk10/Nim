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

int playerTurn(int playID, int waitID, int* piles, int pileNumber);
int checkPilesContent(int* piles, int pileNumber);
int sendPiles(int fd1, int fd2, int* piles, int pileNumber);
void handle_sigchld(int);
int sendMsg(int fd, char* msg);
int shutdownClients(int fd1, int fd2, int __how);

int main (int argc, char **argv)
{
  // signal handler
  signal(SIGCHLD, handle_sigchld);

  // open socket
  int sock = socket(AF_LOCAL, SOCK_STREAM, 0);
  if(sock == -1) {
    fprintf(stderr, "Error 01. Socket error.\n");
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
    fprintf(stderr, "Error 02. Bind error.\n");
    return 2;
  }

  // start listening
  listen(sock, 4);
  fprintf(stderr, "Listening.\n");

  // listen loop
  while (1)
  {
    int test;
    struct sockaddr_un client_addr;
    socklen_t client_len = sizeof(client_addr);
    int fd1 = accept(sock, (struct sockaddr *)&client_addr, &client_len);
    if(fd1 == -1) {
      fprintf(stderr, "PID: %d. Error 03. Socket error.\n", getpid());
      return 3;
    }
    char waitmsg1[] = "Connected. You are Player 1. Waiting for Player 2...\n";
    test = sendMsg(fd1, waitmsg1);
    if (test == -1){
      fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
      return 4;
    }

    struct sockaddr_un client2_addr;
    socklen_t client2_len = sizeof(client2_addr);
    int fd2 = accept(sock, (struct sockaddr *)&client2_addr, &client2_len);
    if(fd2 == -1) {
      fprintf(stderr, "PID: %d. Error 03. Socket error.\n", getpid());
      return 3;
    }
    char waitmsg2[] = "Connected. You are Player 2, please wait for Player 1 to choose the number of piles.\n";
    test = sendMsg(fd2, waitmsg2);
    if (test == -1){
      fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
      shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
      return 4;
    }
    /*
     * forks when both clients are connected
     */
    pid_t pid = fork();
    if(pid == -1) {
      fprintf(stderr, "PID: %d. Error 05. Fork error.\n", getpid());
      shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
      return 5;
    }

    // child handles game
    if(!pid) {
        fprintf(stderr, "Opened connection (PID %d).\n",getpid());

        char greet[] = "Welcome to C_Nim!\n";
        int greetLen = strlen(greet) + 1;
        int pileNumber;
        int player1 = 1;
        int player2 = 2;

        int validSignal = 83;
        int invalidSignal = 84;

        test = sendMsg(fd1, greet); //Send greet to fd1
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }
        test = sendMsg(fd2, greet); //Sent greet to fd2
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }
        test = send(fd1, &player1, sizeof(player1), MSG_NOSIGNAL); // send player ID to fd1
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }
        test = send(fd2, &player2, sizeof(player2), MSG_NOSIGNAL); // send player ID to fd2
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }
        /*
        *
        * Recieve from Player 1 the number of piles to play with and send it to Player
        * 
        */
        while(1){ //Validate reply only if more than (1) piles are selected
          test = recv(fd1, &pileNumber, sizeof(pileNumber), 0);
          if (test == -1){
            fprintf(stderr, "PID: %d. Error 06. No message received.\n", getpid());
            shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
            return 6;
          }
          if (pileNumber < 2){
            test = send(fd1, &invalidSignal, sizeof(invalidSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = sendMsg(fd1, "Please select more than 1 pile.\n");
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
          } else {
            test = send(fd1, &validSignal, sizeof(validSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            break;
          }
        }        
        test = send(fd2, &pileNumber, sizeof(pileNumber), MSG_NOSIGNAL);
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }

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
        test = sendPiles(fd1, fd2, piles, pileNumber);
        if (test == -1){
          fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
          shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
          return 4;
        }
        while(1){
          //Player 1 turn
          test = playerTurn(fd1, fd2, piles, pileNumber);
          if (test == -1){
            fprintf(stderr, "PID: %d. Error 09. Player turn error.\n", getpid());
            shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
            return 9;
          }
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("PID: %d. Player 1 has won, initiating ending sequence.\n", getpid());
            //send end signal
            test = send(fd1, &endSignal, sizeof(endSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = send(fd2, &endSignal, sizeof(endSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            //send win message
            test = sendMsg(fd1, player1WinMsg);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = sendMsg(fd2, player1WinMsg);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            break;
          } else {
            printf("PID: %d. Game is continuing.\n", getpid());
            //send cont signal
            test = send(fd1, &contSignal, sizeof(contSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = send(fd2, &contSignal, sizeof(contSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
          }

          //Player 2 turn
          test = playerTurn(fd2, fd1, piles, pileNumber);
          if (test == -1){
            fprintf(stderr, "PID: %d. Error 09. Player turn error.\n", getpid());
            shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
            return 9;
          }
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("PID %d. Player 2 has won, initiating ending sequence.\n", getpid());
            //send end signal
            test = send(fd1, &endSignal, sizeof(endSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = send(fd2, &endSignal, sizeof(endSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            //send win message
            test = sendMsg(fd1, player2WinMsg);if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = sendMsg(fd2, player2WinMsg);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            break;
          } else {
            printf("PID: %d. Game is continuing.\n", getpid());
            //send cont signal
            test = send(fd1, &contSignal, sizeof(contSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
            test = send(fd2, &contSignal, sizeof(contSignal), MSG_NOSIGNAL);
            if (test == -1){
              fprintf(stderr, "PID: %d. Error 04. Couldn't send message.\n", getpid());
              shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
              return 4;
            }
          }
        }


        shutdownClients(fd1, fd2, SHUT_RDWR); // Close connection with clients
        break;
    }
  }
}

/*
* Function for the player turn. If it's the player's turn, they will choose the elements to remove from the desired pile
* if it's not the player turn, they will wait.
* @param playID, the fd of the player taking their turn
* @param waitID, the fd of the player waiting
* @param piles, the piles in the game
* @param pileNumber, the number of piles in the game
* @param 0 or -1 if any errors have been encountered
*/

int playerTurn(int playID, int waitID, int* piles, int pileNumber){
  char playerTurnPile[] = "Please choose a pile.\n";
  char playerTurnElements[] = "Please choose how many elements to remove.\n";
  char playerTurnWait[] = "Please wait for the other player's turn.\n";

  int chosenPile, chosenElements;

  int play = 1;
  int wait = 0;

  int validSignal = 85;
  int invalidSignal = 86;

  int test;

  //Send play signal
  test = send(playID, &play, sizeof(play), MSG_NOSIGNAL);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  printf("Play signal sent.\n");
  //Send wait signal
  test = send(waitID, &wait, sizeof(wait), MSG_NOSIGNAL);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  printf("Wait signal sent.\n");

  //Send wait message to Player waiting
  test = sendMsg(waitID, playerTurnWait);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  printf("Wait message sent.\n");
  //Ask Player 1 which pile to take from
  test = sendMsg(playID, playerTurnPile);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  printf("PID: %d. Pile message sent.\n", getpid());
  while(1){
    test = recv(playID, &chosenPile, sizeof(chosenPile), 0);
    if (test == -1) {
      fprintf(stderr, "PID: %d. Error 08. No message received.\n");
      return -1;
    }
    if (chosenPile < 0 || chosenPile > pileNumber - 1){ //Check that the chosen pile is valid
      test = send(playID, &invalidSignal, sizeof(invalidSignal), MSG_NOSIGNAL);
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
      test = sendMsg(playID, "Please select a valid pile.\n");
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
    } else {
      test = send(playID, &validSignal, sizeof(validSignal), MSG_NOSIGNAL);
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
      break;
    }
  }
  //Ask Player 1 how many elements to take
  test = sendMsg(playID, playerTurnElements);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  printf("PID: %d. Elements message sent.\n", getpid());
  while(1){
    test = recv(playID, &chosenElements, sizeof(chosenElements), 0);
    if (test == -1) {
      fprintf(stderr, "PID: %d. Error 08. No message received.\n", getpid());
      return -1;
    }
    if (chosenElements < 0 || chosenElements > piles[chosenPile]){ //Check that the number of elements chosen is valid
      test = send(playID, &invalidSignal, sizeof(invalidSignal), MSG_NOSIGNAL);
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
      test = sendMsg(playID, "Please select a number between 0 and the current elements in the pile.\n");
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
    } else {
      test = send(playID, &validSignal, sizeof(validSignal), MSG_NOSIGNAL);
      if (test == -1){
          fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
          return -1;
      }
      break;
    }
  }

  piles[chosenPile] -= chosenElements;
  //send piles to clients
  test = sendPiles(playID, waitID, piles, pileNumber);
  printf("Pile %d is now %d.\n", chosenPile, piles[chosenPile]);
  if (test == -1){
      fprintf(stderr, "PID: %d. Error 07. Couldn't send message.\n", getpid());
      return -1;
  }
  return 0;
}

/*
* Checks if there's at least one piles with more than 0 elements
* @param piles, the piles in the game
* @param pileNumber, the number of piles in the game
* @return 0 if all the piles are empty, 1 if otherwise
*/
int checkPilesContent(int* piles, int pileNumber){
  int result = 0;
  //check piles for a non-zero pile
  for (int i = 0; i < pileNumber; i++){
    if (piles[i] > 0){
      result = 1; //result changed to 1 if such pile is found
    }
  }
  printf("PID: %d. Res: %d\n", result, getpid());
  return result;
}

void handle_sigchld(int x) {
    int saved_errno = errno;
    while (waitpid(-1, 0, WNOHANG) > 0) {}
    errno = saved_errno;
}

/*
* Sends the current piles to both clients connect
* @param fd1, fd2, the clients connected
* @param piles, the piles in the game
* @param pileNumber, the number of piles in the game
* @return 0 or -1 if have any errors have been encountered
*/
int sendPiles(int fd1, int fd2, int* piles, int pileNumber){
  int test;
  //Send signal to validate start
  int signal = 92;
  test = send(fd1, &signal, sizeof(signal), MSG_NOSIGNAL);
  if (test == -1){
    return -1;
  }
  printf("PID: %d. Sent pile signal to fd1\n", getpid());
  test = send(fd2, &signal, sizeof(signal), MSG_NOSIGNAL);
  if (test == -1){
    return -1;
  }
  printf("PID: %d. Sent pile signal to fd2\n", getpid());
  //Send piles
  for (int i = 0; i < pileNumber; i++){
    test = send(fd1, &piles[i], sizeof(piles[i]), MSG_NOSIGNAL);
    if (test == -1){
      return -1;
    }
    printf("PID: %d. Sent pile %d to fd1\n", getpid(), i);
    test = send(fd2, &piles[i], sizeof(piles[i]), MSG_NOSIGNAL);
    if (test == -1){
      return -1;
    }
    printf("PID: %d. Sent pile %d to fd1\n", getpid(), i);
  }
  return 0;
}

/* 
* Sends a string to a client
* @param fd, the client to send the message to
* @param msg, the message to send
* @return 0 or -1 if have any errors have been encountered
*/
int sendMsg(int fd, char* msg){
  int test;
  int msgLen = strlen(msg) + 1;
  test = send(fd, &msgLen, sizeof(msgLen), MSG_NOSIGNAL); // Send msg length
  if (test == -1){ // return -1 if a send error has been encountered
    return -1;
  }
  test = send(fd, msg, msgLen, MSG_NOSIGNAL); // Send msg
  if (test == -1){
    return -1;
  }
  return 0;
}
/*
* Shuts down clients
* @param fd1, fd2, the socket fd of the clients to shut down
* @param __how the flag to use (refer to shutdown() for the flags)
* @return 0 if no issues have been encountered, -1 otherwise
*/
int shutdownClients(int fd1, int fd2, int __how){
  int test;
  test = shutdown(fd1, __how);
  if (test == -1){
    return -1;
  }
  test = shutdown(fd2, __how);
  if (test == -1){
    return -1;
  }
  return 0;
}