/* comum.h - estruturas e constantes compartilhadas entre os processos */
#ifndef COMUM_H
#define COMUM_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define N_ANDARES   8
#define N_ELEV      3
#define CAPACIDADE  4      /* passageiros por elevador (semáforo contador) */
#define MAX_FILA    16     /* passageiros esperando por andar */
#define TICK_MS     600    /* tempo para percorrer um andar */
#define PAUSA_MS    500    /* tempo de porta aberta */

#define SHM_NOME "/elev_shm"
#define MQ_NOME  "/elev_mq"

/* Mensagem enviada pelo processo "passageiros" via fila de mensagens POSIX */
typedef struct { int origem, destino; } Pedido;

typedef struct { int destino; double t0; } Passageiro;

typedef struct {
    int  andar;
    int  alvo;                  /* -1 = sem alvo */
    int  ocupacao;
    int  destinos[N_ANDARES];   /* quantos passageiros descem em cada andar */
    char estado;                /* P parado, S subindo, D descendo, E embarque */
    sem_t vagas;                /* semáforo contador: vagas livres na cabine */
    int  entregues;
} Elevador;

/* Estado global em memória compartilhada (shm_open + mmap) */
typedef struct {
    pthread_mutex_t mtx;            /* process-shared */
    pthread_cond_t  novo_pedido;    /* acorda elevadores ociosos */
    Elevador   elev[N_ELEV];
    Passageiro fila[N_ANDARES][MAX_FILA];
    int        n_fila[N_ANDARES];
    int        reservado[N_ANDARES]; /* id do elevador que vai atender o andar, ou -1 */
    int        total_pedidos, total_entregues, descartados;
    double     soma_espera;
    int        n_espera;
} Estado;

static inline double agora(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
static inline void msleep(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#endif
