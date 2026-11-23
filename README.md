# Simulador de Elevadores Concorrente (C / POSIX)

Projeto da disciplina de Sistemas Operacionais — programação concorrente, sincronização e IPC.

## Objetivo
Simular um prédio com **8 andares** e **3 elevadores** atendendo passageiros que chegam de forma aleatória.
Cada elevador é uma thread, os passageiros são gerados por um processo separado e um painel em outro
processo desenha tudo no terminal em tempo real. O problema do dia a dia (várias "máquinas" disputando
as mesmas chamadas) é uma aplicação direta de concorrência e sincronização.

## Como executar
Requisitos: Linux (ou WSL), `gcc`, `make`. Terminal com pelo menos 80x22 e suporte a ANSI.

```bash
git clone <URL-DO-REPOSITORIO>
cd simulador-elevadores
make
./elevador          # roda até Ctrl+C
./elevador 60       # roda por 60 segundos e mostra o resumo
./corrida           # demo de condição de corrida (sem mutex x com mutex)
```
Constantes (andares, nº de elevadores, capacidade, velocidade) ficam em `comum.h`.

## Arquitetura
```
                  fork + exec                          fork + exec
   ┌───────────────────────────────┐   ┌──────────────────┐   ┌──────────────┐
   │ elevador (processo principal) │   │   passageiros    │   │    painel    │
   │  ┌────────────┐               │   │ gera pedidos     │   │ desenha o    │
   │  │ despachante│◄──────────────┼───┤ aleatórios       │   │ prédio       │
   │  │  (thread)  │  fila de msgs │   └──────────────────┘   └──────┬───────┘
   │  └─────┬──────┘  POSIX (mq)   │                                 │ lê
   │        │ grava                │       ┌─────────────────────────▼───────┐
   │  ┌─────▼──────────────────────┼──────►│ Memória compartilhada (shm+mmap)│
   │  │ Elevador 0,1,2 (3 threads) │       │ mutex + cond + semáforos + estado│
   │  └────────────────────────────┘       └─────────────────────────────────┘
   └───────────────────────────────┘
```

| Arquivo | Papel |
|---|---|
| `elevador.c` | Processo principal: cria shm e fila, lança filhos, threads dos elevadores e do despachante, encerra tudo |
| `passageiros.c` | Processo produtor: envia `Pedido{origem,destino}` pela fila de mensagens |
| `painel.c` | Processo de visualização: lê a memória compartilhada e anima o terminal |
| `corrida.c` | Demonstração de condição de corrida e da correção com mutex |
| `comum.h` | Estruturas compartilhadas e constantes |

## Mecanismos usados
| Conceito | Onde |
|---|---|
| **Processos**: `fork`, `exec`, `wait/waitpid`, `exit` | `elevador.c` lança `painel` e `passageiros` e espera o término |
| **Threads POSIX**: `pthread_create/join` | 3 threads de elevador + 1 de despachante |
| **Mutex** (`pthread_mutex`, *process-shared*) | Protege todo o estado em memória compartilhada (filas, reservas, estatísticas) |
| **Variável de condição** (`pthread_cond`, *process-shared*) | Elevadores ociosos dormem até chegar um novo pedido (`cond_timedwait` / `broadcast`) |
| **Semáforo POSIX** (`sem_init/trywait/post`) | Um semáforo contador por elevador = vagas na cabine (capacidade 4) |
| **Fila de mensagens POSIX** (`mq_send`, `mq_timedreceive`) | `passageiros` → `despachante` |
| **Memória compartilhada POSIX** (`shm_open`, `mmap`) | Estado do prédio compartilhado entre `elevador` e `painel` |
| **Sinais** (`sigaction`, `kill`) | `SIGINT/SIGTERM` para encerramento ordenado de todos os processos |
| **Produtor–consumidor** | `passageiros` (produtor) → fila de mensagens → despachante (consumidor) |
| **Condição de corrida** | `corrida.c` mostra perda de atualizações sem mutex e a solução |

## Decisões de projeto
- **Reserva de chamadas**: um elevador vazio "reserva" o andar que vai atender (`reservado[]`), evitando que dois elevadores corram para o mesmo andar.
- Elevadores com passageiros vão ao destino mais próximo e embarcam quem estiver esperando pelo caminho (se houver vaga).
- Encerramento: threads usam *timeouts* (`timedwait`/`timedreceive`) para checar a flag de parada; recursos IPC são removidos (`mq_unlink`, `shm_unlink`).

## Equipe
- Nome 1 — matrícula
- Nome 2 — matrícula
- Nome 3 — matrícula
