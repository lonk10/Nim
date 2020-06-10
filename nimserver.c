#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h> // per struct sockaddr_un
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>


#include "common.h"

#define RANMAX 50;

void playerTurn(int playID, int waitID, int* piles, int pileNumber);
int checkPilesContent(int* piles, int pileNumber);
void sendPiles(int fd1, int fd2, int* piles, int pileNumber);
void handle_sigchld(int);

int main (int argc, char **argv)
{
  // imposto l'handler per SIGCHLD, in modo da non creare processi zombie
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

  // mi preoccupo di rimuovere il file del socket in caso esista già.
  // se è impegnato da un altro server non si potrà rimuovere, ma bind()
  // successivamente mi darà errore
  unlink(SOCKADDR);
  // lego l'indirizzo al socket di ascolto
  if(bind(sock, (struct sockaddr *)&addr, sizeof addr) == -1) {
    perror("bind()");
    return 2;
  }

  // Abilito effettivamente l'ascolto, con un massimo di 2 client in attesa
  listen(sock, 5);
  fprintf(stderr, "In ascolto.\n");

  // continuo all'infinito ad aspettare client
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
    int waitmsg1len = strlen(waitmsg1) + 1;
    send(fd1, &waitmsg1len, sizeof(waitmsg1len), 0); // Invio lunghezza del messaggio
    send(fd1, waitmsg1, waitmsg1len, 0); // Invio il messaggio

    struct sockaddr_un client2_addr;
    socklen_t client2_len = sizeof(client2_addr);
    int fd2 = accept(sock, (struct sockaddr *)&client2_addr, &client2_len);
    if(fd2 == -1) {
      perror("accept()");
      return 3;
    }
    char waitmsg2[] = "Connected. You are Player 2, please wait for Player 1 to choose the number of piles.\n";
    int waitmsg2len = strlen(waitmsg1) + 1;
    send(fd2, &waitmsg2len, sizeof(waitmsg2len), 0); // Invio lunghezza del messaggio
    send(fd2, waitmsg2, waitmsg2len, 0); // Invio il messaggio


    /*
     * ogni volta che il server accetta una nuova connessione,
     * quest'ultima viene gestita da un nuovo processo figlio
     */
    pid_t pid = fork();
    if(pid == -1) {
      perror("fork()");
      return 4;
    }

    // il figlio gestisce la connessione, il padre torna subito in ascolto
    if(!pid) {
        fprintf(stderr, "Aperta connessione (PID %d).\n",getpid());

        char greet[] = "Benvenuto a C_Nim!\n";
        int greetLen = strlen(greet) + 1;
        int pileNumber;
        int player1 = 1;
        int player2 = 2;

        send(fd1, &greetLen, sizeof(greetLen), 0); // Invio lunghezza del messaggio
        send(fd1, greet, greetLen, 0); // Invio il messaggio
        send(fd2, &greetLen, sizeof(greetLen), 0); // Invio lunghezza del messaggio
        send(fd2, greet, greetLen, 0); // Invio il messaggio
        send(fd1, &player1, sizeof(player1), 0); // Invio player ID
        send(fd2, &player2, sizeof(player2), 0); // Invio player ID
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
        int playerWinMsgLen = strlen(player1WinMsg) + 1;
        int contSignal = 89;
        int endSignal = 90;

        //Send piles to clients
        sendPiles(fd1, fd2, piles, pileNumber);
        while(1){
          //Player 1 turn
          playerTurn(fd1, fd2, piles, pileNumber);
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("Player 1 has won, initiating ending sequence.\n");
            //send end signal
            send(fd1, &endSignal, sizeof(endSignal), 0);
            send(fd2, &endSignal, sizeof(endSignal), 0);
            //send win message
            send(fd1, &playerWinMsgLen, sizeof(playerWinMsgLen), 0);
            send(fd1, player1WinMsg, playerWinMsgLen, 0);
            send(fd2, &playerWinMsgLen, sizeof(playerWinMsgLen), 0);
            send(fd2, player1WinMsg, playerWinMsgLen, 0);
            break;
          } else {
            printf("Game is continuing");
            //send cont signal
            send(fd1, &contSignal, sizeof(contSignal), 0);
            send(fd2, &contSignal, sizeof(contSignal), 0);
          }

          //Player 2 turn
          playerTurn(fd2, fd1, piles, pileNumber);
          if (checkPilesContent(piles, pileNumber) == 0){ //winning condition
            printf("Player 2 has won, initiating ending sequence.\n");
            //send end signal
            send(fd1, &endSignal, sizeof(endSignal), 0);
            send(fd2, &endSignal, sizeof(endSignal), 0);
            //send win message
            send(fd1, &playerWinMsgLen, sizeof(playerWinMsgLen), 0);
            send(fd1, player2WinMsg, playerWinMsgLen, 0);
            send(fd2, &playerWinMsgLen, sizeof(playerWinMsgLen), 0);
            send(fd2, player2WinMsg, playerWinMsgLen, 0);
            break;
          } else {
            printf("Game is continuing.\n");
            //send cont signal
            send(fd1, &contSignal, sizeof(contSignal), 0);
            send(fd2, &contSignal, sizeof(contSignal), 0);
          }
        }


        close(fd2);
        close(fd1); // Alla fine chiudiamo la connessione
    }
  }
}

void playerTurn(int playID, int waitID, int* piles, int pileNumber){
  char playerTurnPile[] = "Please choose a pile.\n";
  char playerTurnElements[] = "Please choose how many elements to remove.\n";
  char playerTurnWait[] = "Please wait for the other player's turn.\n";

  int playerTurnPileLen = strlen(playerTurnPile) + 1;
  int playerTurnElementsLen = strlen(playerTurnElements) + 1;
  int playerTurnWaitLen = strlen(playerTurnWait) + 1;

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
  send(waitID, &playerTurnWaitLen, sizeof(playerTurnWaitLen), 0);
  send(waitID, playerTurnWait, playerTurnWaitLen, 0);
  printf("Wait message sent.\n");
  //Ask Player 1 which pile to take from
  send(playID, &playerTurnPileLen, sizeof(playerTurnPileLen), 0);
  send(playID, playerTurnPile, playerTurnPileLen, 0);
  printf("Pile message sent.\n");
  recv(playID, &chosenPile, sizeof(chosenPile), 0);
  //Ask Player 1 how many elements to take
  send(playID, &playerTurnElementsLen, sizeof(playerTurnElementsLen), 0);
  send(playID,playerTurnElements, playerTurnElementsLen, 0);
  printf("Elements message sent.\n");
  recv(playID, &chosenElements, sizeof(chosenElements), 0);

  piles[chosenPile] -= chosenElements;
  //send piles to clients
  sendPiles(playID, waitID, piles, pileNumber);
  printf("Pile %d is now %d.\n", chosenPile, piles[chosenPile]);
}

int checkPilesContent(int* piles, int pileNumber){
  int result = 0;
  for (int i = 0; i < pileNumber; i++){
    if (piles[i] > 0){
      result = 1;
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