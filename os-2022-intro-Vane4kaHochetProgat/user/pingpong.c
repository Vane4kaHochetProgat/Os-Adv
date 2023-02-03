//
// Created by vane4ka on 19.09.2022.
//

#include "kernel/types.h"
#include "user.h"

int main() {
  int pid;
  int ping[2];
  int pong[2];
  char* response = "ping";
  int processed_number;

  if (pipe(ping) == -1 || pipe(pong) == -1) {
    printf("Pipe creation failure \n");
    exit(-1);
  }

  if ((processed_number = write(ping[1], &response, 4)) != 4) {
    printf("Write failure \n");
    exit(-2);
  }

  if ((pid = fork()) == -1) {
    printf("Process fork failure \n");
    exit(-3);
  }

  switch (pid) {
    case 0:
      if ((processed_number = read(ping[0], &response, 4)) != 4) {
        printf("Read failure \n");
        exit(-2);
      }
      printf("%d: got %s\n", getpid(), response);
      response = "pong";
      if ((processed_number = write(pong[1], &response, 4)) != 4) {
        printf("Write failure \n");
        exit(-2);
      }
      break;
    default:
      if ((processed_number = read(pong[0], &response, 4)) != 4) {
        printf("Read failure \n");
        exit(-2);
      }
      printf("%d: got %s\n", getpid(), response);
  }

  exit(0);
}