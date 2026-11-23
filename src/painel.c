/* painel.c - processo de visualizacao: le a memoria compartilhada e desenha o predio no terminal */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "comum.h"

static volatile sig_atomic_t fim = 0;
static void on_sinal(int s) { (void)s; fim = 1; }

int main(void) {
    struct sigaction sa; memset(&sa, 0, sizeof(sa)); sa.sa_handler = on_sinal;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);

    int fd = -1;
    for (int i = 0; i < 50 && fd < 0; i++) { fd = shm_open(SHM_NOME, O_RDWR, 0600); if (fd < 0) msleep(100); }
    if (fd < 0) { perror("shm_open"); return 1; }
    Estado *E = mmap(NULL, sizeof(Estado), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (E == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);

    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
    printf("\033[2J\033[?25l");

    while (!fim) {
        /* copia um instantaneo consistente sob o mutex */
        int andar[N_ELEV], alvo[N_ELEV], ocup[N_ELEV], ent[N_ELEV], dest[N_ELEV][N_ANDARES];
        char est[N_ELEV]; int nf[N_ANDARES]; int ped, ent_t, desc; double media = 0;
        pthread_mutex_lock(&E->mtx);
        for (int i = 0; i < N_ELEV; i++) {
            andar[i] = E->elev[i].andar; alvo[i] = E->elev[i].alvo; ocup[i] = E->elev[i].ocupacao;
            ent[i] = E->elev[i].entregues; est[i] = E->elev[i].estado;
            memcpy(dest[i], E->elev[i].destinos, sizeof(dest[i]));
        }
        memcpy(nf, E->n_fila, sizeof(nf));
        ped = E->total_pedidos; ent_t = E->total_entregues; desc = E->descartados;
        if (E->n_espera) media = E->soma_espera / E->n_espera;
        pthread_mutex_unlock(&E->mtx);

        printf("\033[H\033[1m SIMULADOR DE ELEVADORES\033[0m  (Ctrl+C para sair)\033[K\n\033[K\n");
        printf("\033[K          Fila de espera     ");
        for (int i = 0; i < N_ELEV; i++) printf("   E%d     ", i);
        printf("\n");
        for (int f = N_ANDARES - 1; f >= 0; f--) {
            printf("\033[K Andar %d  \033[33m", f);
            for (int k = 0; k < MAX_FILA; k++) putchar(k < nf[f] ? 'o' : ' ');
            printf("\033[0m  ");
            for (int i = 0; i < N_ELEV; i++) {
                if (andar[i] == f) {
                    const char *sim = "="; int cor = 36;
                    if (est[i] == 'S') { sim = "^"; cor = 32; }
                    else if (est[i] == 'D') { sim = "v"; cor = 31; }
                    else if (est[i] == 'E') { sim = "#"; cor = 33; }
                    printf("\033[%dm[%s %d/%d]\033[0m   ", cor, sim, ocup[i], CAPACIDADE);
                } else printf("|     |   ");
            }
            printf("\n");
        }
        printf("\033[K\n");
        for (int i = 0; i < N_ELEV; i++) {
            printf("\033[K E%d: andar %d, alvo %s, entregues %d, destinos:", i, andar[i],
                   alvo[i] < 0 ? "-" : (char[2]){ '0' + alvo[i], 0 }, ent[i]);
            for (int f = 0; f < N_ANDARES; f++) if (dest[i][f]) printf(" %d(x%d)", f, dest[i][f]);
            printf("\n");
        }
        printf("\033[K\n\033[K Pedidos: %d | Entregues: %d | Descartados: %d | Espera media: %.1fs\n",
               ped, ent_t, desc, media);
        fflush(stdout);
        msleep(150);
    }
    printf("\033[?25h\n"); fflush(stdout);
    munmap(E, sizeof(Estado));
    return 0;
}
