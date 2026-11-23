/* passageiros.c - processo produtor: gera pedidos aleatorios e envia por fila de mensagens POSIX */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include "comum.h"

int main(void) {
    mqd_t mq = (mqd_t)-1;
    for (int i = 0; i < 50 && mq == (mqd_t)-1; i++) {
        mq = mq_open(MQ_NOME, O_WRONLY);
        if (mq == (mqd_t)-1) msleep(100);
    }
    if (mq == (mqd_t)-1) { perror("mq_open"); return 1; }

    srand(time(NULL) ^ getpid());
    for (;;) {
        Pedido p;
        p.origem = rand() % N_ANDARES;
        do { p.destino = rand() % N_ANDARES; } while (p.destino == p.origem);
        if (mq_send(mq, (const char *)&p, sizeof(p), 0) < 0) { perror("mq_send"); break; }
        msleep(300 + rand() % 1200);
    }
    mq_close(mq);
    return 0;
}
