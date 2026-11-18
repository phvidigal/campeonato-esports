#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "models/campeonato.h"
#include "models/partida.h"
#include "models/equipe.h"
#include "models/jogador.h"
#include "algoritmos/busca.h"
#include "persistencia/banco.h"

/* ============================================================
   LEITURA VALIDADA (parte A)
   ============================================================ */

/* Pede um número inteiro entre min e max. Repete até a pessoa acertar. */
int ler_int(const char *prompt, int min, int max) {
    char linha[64];

    while (1) {
        printf("%s", prompt);
        if (fgets(linha, sizeof(linha), stdin) == NULL)
            exit(0); /* acabou a entrada (Ctrl+D): sai em vez de ficar num loop infinito */

        /* linha maior que o buffer: descarta o resto para não virar outra leitura */
        if (strchr(linha, '\n') == NULL && !feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            printf("Digite apenas números.\n");
            continue;
        }

        char *fim;
        long valor = strtol(linha, &fim, 10);

        /* ignora espaços e o \n no final */
        while (*fim == ' ' || *fim == '\n' || *fim == '\r')
            fim++;

        if (fim == linha || *fim != '\0') {
            printf("Digite apenas números.\n");
        } else if (valor < min || valor > max) {
            printf("Digite um valor entre %d e %d.\n", min, max);
        } else {
            return (int)valor;
        }
    }
}

/* Pede um texto não vazio. Remove o \n e respeita o tamanho do destino. */
void ler_texto(const char *prompt, char *dest, int tam) {
    char linha[256];

    while (1) {
        printf("%s", prompt);
        if (fgets(linha, sizeof(linha), stdin) == NULL)
            exit(0);

        size_t n = strlen(linha);
        if (n > 0 && linha[n - 1] == '\n') {
            linha[n - 1] = '\0';
            if (n > 1 && linha[n - 2] == '\r') linha[n - 2] = '\0';
        } else {
            /* linha maior que o buffer: descarta o resto */
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
        }

        /* só espaços também conta como vazio */
        const char *p = linha;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0') {
            printf("O texto não pode ser vazio.\n");
            continue;
        }

        strncpy(dest, linha, tam - 1);
        dest[tam - 1] = '\0';
        return;
    }
}

/* ============================================================
   MENU DO CAMPEONATO (parte A)
   ============================================================ */

static void mostrar_fase(const Campeonato *c) {
    switch (campeonato_fase_atual(c)) {
        case FASE_CADASTRO:  printf("Fase: Cadastro\n"); break;
        case FASE_SUICO:     printf("Fase: Suíço - Rodada %d\n", campeonato_rodada_atual(c)); break;
        case FASE_SEMI:      printf("Fase: Semifinal\n"); break;
        case FASE_FINAL:     printf("Fase: Final\n"); break;
        case FASE_ENCERRADO: printf("Fase: Encerrado\n"); break;
    }
}

static void gerar_rodada(Campeonato *c, sqlite3 *db) {
    int qtd = 0;
    int r = gerar_proxima_rodada(c, &qtd);
    if (r != OK) {
        printf("Erro: %s\n", msg_erro(r));
        return;
    }

    /* grava no banco só as partidas novas (as últimas qtd do array) */
    for (int i = c->n_partidas - qtd; i < c->n_partidas; i++)
        banco_inserir_partida(db, &c->partidas[i]);

    printf("%d partida(s) gerada(s).\n", qtd);
    listar_partidas_rodada_atual(c);
}

static void registrar_resultado(Campeonato *c, sqlite3 *db) {
    int id = ler_int("Id da partida: ", 1, 9999);
    int a = ler_int("Placar da equipe A: ", 0, 999);
    int b = ler_int("Placar da equipe B: ", 0, 999);

    int r = partida_registrar_resultado(c, id, a, b);
    if (r != OK) {
        printf("Erro: %s\n", msg_erro(r));
        return;
    }

    int pos = busca_linear_partida(c->partidas, c->n_partidas, id);
    banco_atualizar_partida(db, &c->partidas[pos]);
    printf("Resultado registrado.\n");
}

void menu_campeonato(Campeonato *c, sqlite3 *db) {
    int op;
    do {
        printf("\n=== CAMPEONATO ===\n");
        mostrar_fase(c);
        printf("1. Gerar próxima rodada\n");
        printf("2. Partidas da rodada atual\n");
        printf("3. Registrar resultado\n");
        printf("4. Ranking\n");
        printf("5. Matriz de confrontos\n");
        printf("6. Campeão\n");
        printf("0. Voltar\n");
        op = ler_int("Opção: ", 0, 6);

        switch (op) {
            case 1: gerar_rodada(c, db); break;
            case 2: listar_partidas_rodada_atual(c); break;
            case 3: registrar_resultado(c, db); break;
            case 4: exibir_ranking(c); break;
            case 5: exibir_matriz_confrontos(c); break;
            case 6: exibir_campeao(c); break;
        }
    } while (op != 0);
}

/* ============================================================
   MENUS DE EQUIPES E JOGADORES (parte B)
   Padrão: lê, chama o model e, se deu OK, grava no banco.
   ============================================================ */

/* O banco já imprimiu o erro do SQLite; aqui só avisa o usuário. */
static void avisar_banco(int r) {
    if (r != OK)
        printf("Aviso: a alteração não foi gravada no banco.\n");
}

static void cadastrar_equipe(Campeonato *c, sqlite3 *db) {
    char nome[TAM_NOME];
    int id = ler_int("Código da equipe: ", 1, 9999);
    ler_texto("Nome: ", nome, TAM_NOME);

    int r = equipe_cadastrar(c, id, nome);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    avisar_banco(banco_inserir_equipe(db, equipe_buscar(c, id)));
    printf("Equipe cadastrada.\n");
}

static void alterar_equipe(Campeonato *c, sqlite3 *db) {
    char nome[TAM_NOME];
    int id = ler_int("Código da equipe: ", 1, 9999);
    ler_texto("Novo nome: ", nome, TAM_NOME);

    int r = equipe_alterar_nome(c, id, nome);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    avisar_banco(banco_atualizar_equipe(db, equipe_buscar(c, id)));
    printf("Nome alterado.\n");
}

static void excluir_equipe(Campeonato *c, sqlite3 *db) {
    int id = ler_int("Código da equipe: ", 1, 9999);
    if (ler_int("Os jogadores dela também serão excluídos. Confirma? (1 = sim, 0 = não): ", 0, 1) == 0)
        return;

    int r = equipe_excluir(c, id);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    /* no banco, os jogadores saem pelo ON DELETE CASCADE */
    avisar_banco(banco_excluir_equipe(db, id));
    printf("Equipe excluída.\n");
}

static void buscar_equipe(Campeonato *c) {
    int id = ler_int("Código da equipe: ", 1, 9999);
    Equipe *e = equipe_buscar(c, id);
    if (e == NULL) { printf("Erro: %s\n", msg_erro(ERR_NAO_ENCONTRADO)); return; }

    printf("\nEquipe %d - %s | %d pts, %dV %dD, saldo %+d\n",
           e->id, e->nome, e->pontos, e->vitorias, e->derrotas, e->saldo);
    jogador_listar_da_equipe(c, id);
}

void menu_equipes(Campeonato *c, sqlite3 *db) {
    int op;
    do {
        printf("\n=== EQUIPES ===\n");
        printf("1. Cadastrar\n");
        printf("2. Alterar nome\n");
        printf("3. Excluir\n");
        printf("4. Listar\n");
        printf("5. Buscar por id\n");
        printf("0. Voltar\n");
        op = ler_int("Opção: ", 0, 5);

        switch (op) {
            case 1: cadastrar_equipe(c, db); break;
            case 2: alterar_equipe(c, db); break;
            case 3: excluir_equipe(c, db); break;
            case 4: equipe_listar(c); break;
            case 5: buscar_equipe(c); break;
        }
    } while (op != 0);
}

static void cadastrar_jogador(Campeonato *c, sqlite3 *db) {
    char nick[TAM_NICK], nome[TAM_NOME];
    int id;
    ler_texto("Nick: ", nick, TAM_NICK);
    ler_texto("Nome: ", nome, TAM_NOME);
    int id_equipe = ler_int("Código da equipe: ", 1, 9999);

    int r = jogador_cadastrar(c, nick, nome, id_equipe, &id);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    avisar_banco(banco_inserir_jogador(db, jogador_buscar_nick(c, nick)));
    printf("Jogador cadastrado com id %d.\n", id);
}

/* O usuário identifica o jogador pelo nick; o model trabalha com o id. */
static void alterar_jogador(Campeonato *c, sqlite3 *db) {
    char nick[TAM_NICK], nome[TAM_NOME];
    ler_texto("Nick do jogador: ", nick, TAM_NICK);
    Jogador *j = jogador_buscar_nick(c, nick);
    if (j == NULL) { printf("Erro: %s\n", msg_erro(ERR_NAO_ENCONTRADO)); return; }

    printf("Atual: %s (equipe %d)\n", j->nome, j->id_equipe);
    ler_texto("Novo nome: ", nome, TAM_NOME);
    int id_equipe = ler_int("Novo código da equipe: ", 1, 9999);

    int r = jogador_alterar(c, j->id, nome, id_equipe);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    avisar_banco(banco_atualizar_jogador(db, j));
    printf("Jogador alterado.\n");
}

static void excluir_jogador(Campeonato *c, sqlite3 *db) {
    char nick[TAM_NICK];
    ler_texto("Nick do jogador: ", nick, TAM_NICK);
    Jogador *j = jogador_buscar_nick(c, nick);
    if (j == NULL) { printf("Erro: %s\n", msg_erro(ERR_NAO_ENCONTRADO)); return; }

    int id = j->id;   /* guarda antes: o excluir desloca o array */
    int r = jogador_excluir(c, id);
    if (r != OK) { printf("Erro: %s\n", msg_erro(r)); return; }

    avisar_banco(banco_excluir_jogador(db, id));
    printf("Jogador excluído.\n");
}

static void buscar_jogador(Campeonato *c) {
    char nick[TAM_NICK];
    ler_texto("Nick: ", nick, TAM_NICK);
    Jogador *j = jogador_buscar_nick(c, nick);
    if (j == NULL) { printf("Erro: %s\n", msg_erro(ERR_NAO_ENCONTRADO)); return; }

    Equipe *e = equipe_buscar(c, j->id_equipe);
    printf("\nJogador %d: %s (%s) - equipe %d %s\n",
           j->id, j->nick, j->nome, j->id_equipe, (e == NULL) ? "?" : e->nome);
}

void menu_jogadores(Campeonato *c, sqlite3 *db) {
    int op;
    do {
        printf("\n=== JOGADORES ===\n");
        printf("1. Cadastrar\n");
        printf("2. Alterar\n");
        printf("3. Excluir\n");
        printf("4. Listar por nick\n");
        printf("5. Listar por equipe\n");
        printf("6. Buscar por nick\n");
        printf("0. Voltar\n");
        op = ler_int("Opção: ", 0, 6);

        switch (op) {
            case 1: cadastrar_jogador(c, db); break;
            case 2: alterar_jogador(c, db); break;
            case 3: excluir_jogador(c, db); break;
            case 4: jogador_listar_por_nick(c); break;
            case 5: jogador_listar_da_equipe(c, ler_int("Código da equipe: ", 1, 9999)); break;
            case 6: buscar_jogador(c); break;
        }
    } while (op != 0);
}

/* ============================================================
   MAIN
   ============================================================ */

int main(void) {
    Campeonato c;
    sqlite3 *db = NULL;

    campeonato_init(&c);

    if (banco_abrir(&db, "esports.db") != OK) {
        printf("Não foi possível abrir o banco.\n");
        return 1;
    }
    banco_carregar(db, &c);
    recalcular_classificacao(&c);

    int op;
    do {
        printf("\n=== CAMPEONATO E-SPORTS ===\n");
        mostrar_fase(&c);
        printf("1. Equipes\n");
        printf("2. Jogadores\n");
        printf("3. Campeonato\n");
        printf("0. Sair\n");
        op = ler_int("Opção: ", 0, 3);

        switch (op) {
            case 1: menu_equipes(&c, db); break;
            case 2: menu_jogadores(&c, db); break;
            case 3: menu_campeonato(&c, db); break;
        }
    } while (op != 0);

    banco_fechar(db);
    return 0;
}
