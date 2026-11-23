/* corrida.c - demonstracao de condicao de corrida: contador SEM mutex x COM mutex */
#include <stdio.h>
#include <pthread.h>
#include <sched.h>

#define N_THREADS 4
#define ITER 1000000

static volatile long contador;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

static void *sem_mutex(void *a) { (void)a; for (int i = 0; i < ITER; i++) {
        long tmp = contador;            /* le */
        if (i % 50 == 0) sched_yield(); /* abre janela para outra thread intercalar */
        contador = tmp + 1;             /* escreve (pode sobrescrever o valor de outra thread) */
    }
    return NULL; }
static void *com_mutex(void *a) {
    (void)a;
    for (int i = 0; i < ITER; i++) { pthread_mutex_lock(&m); contador++; pthread_mutex_unlock(&m); }
    return NULL;
}
static void roda(void *(*f)(void *), const char *nome) {
    pthread_t t[N_THREADS]; contador = 0;
    for (int i = 0; i < N_THREADS; i++) pthread_create(&t[i], NULL, f, NULL);
    for (int i = 0; i < N_THREADS; i++) pthread_join(t[i], NULL);
    printf("%-11s esperado=%ld obtido=%ld  %s\n", nome, (long)N_THREADS * ITER, contador,
           contador == (long)N_THREADS * ITER ? "(OK)" : "(PERDEU ATUALIZACOES!)");
}
int main(void) { roda(sem_mutex, "SEM mutex:"); roda(com_mutex, "COM mutex:"); return 0; }
