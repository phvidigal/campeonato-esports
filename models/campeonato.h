#ifndef CAMPEONATO_H
#define CAMPEONATO_H

/* ---------- Constantes ---------- */
#define MAX_EQ 8
#define MAX_JOG 40
#define JOG_POR_EQ 5
#define MAX_PART 15 /* 12 suíço + 2 semi + 1 final */
#define RODADAS_SUICO 3
#define TAM_NOME 50
#define TAM_NICK 30

/* Fases (campo Partida.fase e retorno de campeonato_fase_atual) */
#define FASE_CADASTRO 0
#define FASE_SUICO 1
#define FASE_SEMI 2
#define FASE_FINAL 3
#define FASE_ENCERRADO 4

/* Códigos de retorno de toda função que altera dados */
#define OK 0
#define ERR_DUPLICADO 1
#define ERR_NAO_ENCONTRADO 2
#define ERR_CHEIO 3
#define ERR_INVALIDO 4
#define ERR_BLOQUEADO 5 /* operação não permitida na fase atual */
#define ERR_PENDENTE 6  /* rodada atual tem partida sem resultado */

/* ---------- Structs ---------- */
typedef struct {
    int id;              /* digitado pelo usuário, único, > 0 */
    char nome[TAM_NOME];
    int pontos;          /* calculado */
    int vitorias;        /* calculado */
    int derrotas;        /* calculado */
    int saldo;           /* calculado: rounds feitos - sofridos */
} Equipe;

typedef struct {
    int id;              /* gerado: c->prox_id_jogador++ */
    char nick[TAM_NICK]; /* único */
    char nome[TAM_NOME];
    int id_equipe;       /* id, não ponteiro: ponteiro não sobrevive ao banco */
} Jogador;

typedef struct {
    int id;              /* gerado: c->prox_id_partida++ */
    int fase;            /* FASE_SUICO, FASE_SEMI ou FASE_FINAL */
    int rodada;          /* 1..3 no suíço; 0 no mata-mata */
    int id_a, id_b;
    int placar_a, placar_b;
    int finalizada;      /* 0 = aguardando resultado, 1 = com resultado */
} Partida;

typedef struct {
    Equipe equipes[MAX_EQ];       int n_equipes;   /* SEMPRE ordenado por id */
    Jogador jogadores[MAX_JOG];   int n_jogadores;
    Partida partidas[MAX_PART];   int n_partidas;  /* ordem de criação */
    int confronto[MAX_EQ][MAX_EQ]; /* calculado. índice = posição em equipes[] */
                                   /* 1 = i venceu j, -1 = i perdeu de j, 0 = não jogaram */
    int prox_id_jogador;
    int prox_id_partida;
} Campeonato;

/* ---------- Funções (parte A) ---------- */
void campeonato_init(Campeonato *c);                    /* zera tudo; prox_id = 1 */
int campeonato_fase_atual(const Campeonato *c);         /* deduz a fase pela última partida */
int campeonato_rodada_atual(const Campeonato *c);       /* rodada da última partida; 0 se nenhuma */
int campeonato_rodada_concluida(const Campeonato *c);   /* 1 se todas as partidas da rodada atual têm resultado */
const char *msg_erro(int codigo);                       /* texto para cada ERR_* */

#endif
