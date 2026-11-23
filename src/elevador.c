/* elevador.c - processo principal: cria IPC, threads dos elevadores e lança os outros processos */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <mqueue.h>
#include "comum.h"

static Estado *E;
static mqd_t mq;
static volatile sig_atomic_t parar = 0;

static void on_sinal(int s) { (void)s; parar = 1; }

/* espera por novo pedido (mutex deve estar travado); timeout para checar 'parar' */
static void espera_pedido(void) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 300000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&E->novo_pedido, &E->mtx, &ts);
}

/* Thread consumidora: lê pedidos da fila de mensagens e insere na fila do andar */
static void *thread_despachante(void *arg) {
    (void)arg;
    Pedido p;
    while (!parar) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 300000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        ssize_t n = mq_timedreceive(mq, (char *)&p, sizeof(p), NULL, &ts);
        if (n != (ssize_t)sizeof(p)) continue;            /* timeout ou EINTR */
        if (p.origem < 0 || p.origem >= N_ANDARES || p.destino < 0 || p.destino >= N_ANDARES) continue;

        pthread_mutex_lock(&E->mtx);
        E->total_pedidos++;
        if (E->n_fila[p.origem] < MAX_FILA) {
            Passageiro ps = { p.destino, agora() };
            E->fila[p.origem][E->n_fila[p.origem]++] = ps;
            pthread_cond_broadcast(&E->novo_pedido);
        } else {
            E->descartados++;
        }
        pthread_mutex_unlock(&E->mtx);
    }
    return NULL;
}

/* Uma thread por elevador */
static void *thread_elevador(void *arg) {
    int id = (int)(long)arg;
    Elevador *e = &E->elev[id];

    pthread_mutex_lock(&E->mtx);
    while (!parar) {
        int mexeu = 0, a = e->andar;

        /* 1) desembarque: libera vagas no semáforo */
        int n = e->destinos[a];
        if (n > 0) {
            for (int i = 0; i < n; i++) sem_post(&e->vagas);
            e->ocupacao -= n; e->entregues += n; E->total_entregues += n;
            e->destinos[a] = 0; mexeu = 1;
        }

        /* 2) embarque: só entra quem conseguir uma vaga (sem_trywait) */
        while (E->n_fila[a] > 0 && sem_trywait(&e->vagas) == 0) {
            Passageiro p = E->fila[a][0];
            memmove(&E->fila[a][0], &E->fila[a][1], (E->n_fila[a] - 1) * sizeof(Passageiro));
            E->n_fila[a]--;
            e->destinos[p.destino]++; e->ocupacao++;
            E->soma_espera += agora() - p.t0; E->n_espera++;
            mexeu = 1;
        }
        if (E->reservado[a] == id) E->reservado[a] = -1;

        if (mexeu) {
            e->estado = 'E'; e->alvo = -1;
            pthread_mutex_unlock(&E->mtx); msleep(PAUSA_MS); pthread_mutex_lock(&E->mtx);
        }

        /* 3) escolhe alvo: com passageiros -> destino mais próximo;
              vazio -> andar com fila mais próximo que ninguém reservou */
        for (int f = 0; f < N_ANDARES; f++) if (E->reservado[f] == id) E->reservado[f] = -1;
        int alvo = -1, melhor = 1000;
        for (int f = 0; f < N_ANDARES; f++) {
            int d = abs(f - e->andar);
            int candidato = (e->ocupacao > 0) ? (e->destinos[f] > 0)
                                              : (E->n_fila[f] > 0 && E->reservado[f] == -1);
            if (candidato && d < melhor) { melhor = d; alvo = f; }
        }
        if (alvo >= 0 && e->ocupacao == 0) E->reservado[alvo] = id;
        e->alvo = alvo;

        if (alvo < 0) { e->estado = 'P'; espera_pedido(); continue; }
        if (alvo == e->andar) continue;                 /* novo passageiro chegou neste andar */

        /* 4) move um andar */
        int dir = (alvo > e->andar) ? 1 : -1;
        e->estado = (dir > 0) ? 'S' : 'D';
        pthread_mutex_unlock(&E->mtx); msleep(TICK_MS); pthread_mutex_lock(&E->mtx);
        e->andar += dir;
    }
    pthread_mutex_unlock(&E->mtx);
    return NULL;
}

static pid_t lanca(const char *prog) {
    pid_t p = fork();
    if (p == 0) { execl(prog, prog, (char *)NULL); perror(prog); _exit(127); }
    return p;
}

int main(int argc, char **argv) {
    int duracao = (argc > 1) ? atoi(argv[1]) : 0;       /* segundos; 0 = até Ctrl+C */

    struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_sinal;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    /* memória compartilhada */
    shm_unlink(SHM_NOME);
    int fd = shm_open(SHM_NOME, O_CREAT | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, sizeof(Estado)) < 0) { perror("ftruncate"); return 1; }
    E = mmap(NULL, sizeof(Estado), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (E == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);
    memset(E, 0, sizeof(Estado));

    pthread_mutexattr_t ma; pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&E->mtx, &ma);
    pthread_condattr_t ca; pthread_condattr_init(&ca);
    pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&E->novo_pedido, &ca);

    for (int i = 0; i < N_ELEV; i++) {
        E->elev[i].andar = i * (N_ANDARES - 1) / (N_ELEV - 1);
        E->elev[i].alvo = -1; E->elev[i].estado = 'P';
        sem_init(&E->elev[i].vagas, 1, CAPACIDADE);
    }
    for (int f = 0; f < N_ANDARES; f++) E->reservado[f] = -1;

    /* fila de mensagens */
    mq_unlink(MQ_NOME);
    struct mq_attr at; memset(&at, 0, sizeof(at));
    at.mq_maxmsg = 10; at.mq_msgsize = sizeof(Pedido);
    mq = mq_open(MQ_NOME, O_CREAT | O_RDWR, 0600, &at);
    if (mq == (mqd_t)-1) { perror("mq_open"); return 1; }

    /* processos filhos (fork + exec) */
    pid_t p_painel = lanca("./painel");
    pid_t p_pass   = lanca("./passageiros");

    pthread_t te[N_ELEV], td;
    for (long i = 0; i < N_ELEV; i++) pthread_create(&te[i], NULL, thread_elevador, (void *)i);
    pthread_create(&td, NULL, thread_despachante, NULL);

    for (int t = 0; !parar && (duracao == 0 || t < duracao * 10); t++) msleep(100);

    /* encerramento ordenado */
    parar = 1;
    kill(p_pass, SIGTERM); kill(p_painel, SIGTERM);
    waitpid(p_pass, NULL, 0); waitpid(p_painel, NULL, 0);
    pthread_mutex_lock(&E->mtx); pthread_cond_broadcast(&E->novo_pedido); pthread_mutex_unlock(&E->mtx);
    for (int i = 0; i < N_ELEV; i++) pthread_join(te[i], NULL);
    pthread_join(td, NULL);

    printf("\n=== Resumo ===\nPedidos: %d | Entregues: %d | Descartados (fila cheia): %d\n",
           E->total_pedidos, E->total_entregues, E->descartados);
    if (E->n_espera) printf("Espera media: %.2f s\n", E->soma_espera / E->n_espera);
    for (int i = 0; i < N_ELEV; i++) printf("Elevador %d entregou %d passageiros\n", i, E->elev[i].entregues);

    mq_close(mq); mq_unlink(MQ_NOME);
    for (int i = 0; i < N_ELEV; i++) sem_destroy(&E->elev[i].vagas);
    munmap(E, sizeof(Estado)); shm_unlink(SHM_NOME);
    return 0;
}
