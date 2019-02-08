#include "server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
// #include <windows.h>
#include <time.h>

#define READ_BOARD 1
#define WRITE_BOARD 2
#define UPDATE_SSEG2 3

#define MSG_MAX_SIZE 350
#define BUFFER_SIZE (MSG_MAX_SIZE + 100)
#define LOGIN_MAX_SIZE 13
#define MAX_CHAT_CLIENTS 3

typedef struct {
  int operacao, valor, local;
} Pacote;

Pacote dados;

int main() {
  char client_names[MAX_CHAT_CLIENTS][LOGIN_MAX_SIZE];
  char str_buffer[BUFFER_SIZE], aux_buffer[BUFFER_SIZE];
  serverInit(MAX_CHAT_CLIENTS);
  puts("Server is running!!");
  int leitura;
  int dev = open("/dev/de2i150_altera", O_RDWR);
  dados.local = 0;
  dados.operacao = 0;
  dados.valor = 0;
  while (1) {
    int id = acceptConnection();
    if (id != NO_CONNECTION) {
      recvMsgFromClient(client_names[id], id, WAIT_FOR_IT);
      strcpy(str_buffer, client_names[id]);
      strcat(str_buffer, " connected to chat");
      broadcast(str_buffer, (int)strlen(str_buffer) + 1);
      printf("%s connected id = %d\n", client_names[id], id);
    }

    struct msg_ret_t msg_ret = recvMsg(&dados);
    
    if (msg_ret.status == MESSAGE_OK) {
      switch (dados.operacao) {
        case WRITE_BOARD: {
          write(dev, &dados.valor, dados.local);
          break;
        }
        case READ_BOARD: {
          read(dev, &leitura, dados.local);
          dados.operacao = READ_BOARD;
          dados.valor = leitura;
          dados.local = dados.local;
          broadcast(&leitura, sizeof(int));
          break;
        }
        case UPDATE_SSEG2: {
          write(dev, &dados.valor, dados.local);
          break;
        }
      }
    }
  }
  close(dev);
  return 0;
}
