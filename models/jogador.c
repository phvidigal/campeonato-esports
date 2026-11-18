#include <stdio.h>
#include <string.h>
#include "jogador.h"
#include "../algoritmos/busca.h"
#include "../algoritmos/ordenacao.h"

/* Quantos jogadores a equipe já tem. */
static int qtd_jogadores_equipe(const Campeonato *c, int id_equipe) {
    int indices[MAX_JOG];
    return busca_linear_jogadores_equipe(c->jogadores, c->n_jogadores, id_equipe, indices);
}

int jogador_cadastrar(Campeonato *c, const char *nick, const char *nome, int id_equipe, int *id_gerado) {
    if (nick[0] == '\0' || nome[0] == '\0')
        return ERR_INVALIDO;   /* extra: o ler_texto já recusa vazio */
    if (busca_linear_jogador_nick(c->jogadores, c->n_jogadores, nick) != -1)
        return ERR_DUPLICADO;
    if (busca_binaria_equipe(c->equipes, c->n_equipes, id_equipe) == -1)
        return ERR_NAO_ENCONTRADO;
    if (c->n_jogadores == MAX_JOG || qtd_jogadores_equipe(c, id_equipe) >= JOG_POR_EQ)
        return ERR_CHEIO;

    Jogador *j = &c->jogadores[c->n_jogadores];   /* entra no fim do array */
    j->id = c->prox_id_jogador++;
    strncpy(j->nick, nick, TAM_NICK - 1);
    j->nick[TAM_NICK - 1] = '\0';
    strncpy(j->nome, nome, TAM_NOME - 1);
    j->nome[TAM_NOME - 1] = '\0';
    j->id_equipe = id_equipe;
    c->n_jogadores++;

    *id_gerado = j->id;   /* parâmetro de saída: o return já é o código de erro */
    return OK;
}

/* O nick não muda: é como o usuário identifica o jogador. */
int jogador_alterar(Campeonato *c, int id, const char *novo_nome, int novo_id_equipe) {
    int i = busca_linear_jogador_id(c->jogadores, c->n_jogadores, id);
    if (i == -1)
        return ERR_NAO_ENCONTRADO;
    if (novo_nome[0] == '\0')
        return ERR_INVALIDO;
    if (busca_binaria_equipe(c->equipes, c->n_equipes, novo_id_equipe) == -1)
        return ERR_NAO_ENCONTRADO;

    /* Só conta o limite se ele mudar de equipe (na mesma, ele já é um dos 5). */
    if (c->jogadores[i].id_equipe != novo_id_equipe &&
        qtd_jogadores_equipe(c, novo_id_equipe) >= JOG_POR_EQ)
        return ERR_CHEIO;

    strncpy(c->jogadores[i].nome, novo_nome, TAM_NOME - 1);
    c->jogadores[i].nome[TAM_NOME - 1] = '\0';
    c->jogadores[i].id_equipe = novo_id_equipe;
    return OK;
}

int jogador_excluir(Campeonato *c, int id) {
    int i = busca_linear_jogador_id(c->jogadores, c->n_jogadores, id);
    if (i == -1)
        return ERR_NAO_ENCONTRADO;

    for (int k = i; k < c->n_jogadores - 1; k++)   /* desloca à esquerda */
        c->jogadores[k] = c->jogadores[k + 1];
    c->n_jogadores--;
    return OK;
}

/* Imprime um jogador com o nome da equipe. */
static void imprimir_jogador(const Campeonato *c, const Jogador *j) {
    int e = busca_binaria_equipe(c->equipes, c->n_equipes, j->id_equipe);
    printf("%-4d %-20s %-25s %d - %s\n", j->id, j->nick, j->nome,
           j->id_equipe, (e == -1) ? "?" : c->equipes[e].nome);
}

/* Ordena um array de PONTEIROS: c->jogadores[] não muda de ordem. */
void jogador_listar_por_nick(const Campeonato *c) {
    Jogador *v[MAX_JOG];

    if (c->n_jogadores == 0) {
        printf("\nNenhum jogador cadastrado.\n");
        return;
    }

    /* cast só porque c é const: a ordenação mexe nos ponteiros, não nos jogadores */
    for (int i = 0; i < c->n_jogadores; i++)
        v[i] = (Jogador *)&c->jogadores[i];
    insertion_sort_jogadores_nick(v, c->n_jogadores);

    printf("\n--- JOGADORES POR NICK ---\n");
    printf("Id   %-20s %-25s Equipe\n", "Nick", "Nome");
    for (int i = 0; i < c->n_jogadores; i++)
        imprimir_jogador(c, v[i]);
}

void jogador_listar_da_equipe(const Campeonato *c, int id_equipe) {
    int indices[MAX_JOG];
    int e = busca_binaria_equipe(c->equipes, c->n_equipes, id_equipe);

    if (e == -1) {
        printf("\nEquipe %d não existe.\n", id_equipe);
        return;
    }

    int qtd = busca_linear_jogadores_equipe(c->jogadores, c->n_jogadores, id_equipe, indices);
    printf("\n--- JOGADORES DA EQUIPE %d - %s (%d/%d) ---\n",
           id_equipe, c->equipes[e].nome, qtd, JOG_POR_EQ);
    if (qtd == 0) {
        printf("Nenhum jogador nesta equipe.\n");
        return;
    }
    for (int k = 0; k < qtd; k++)
        imprimir_jogador(c, &c->jogadores[indices[k]]);
}

/* Devolve o endereço do jogador dentro do array (ou NULL). */
Jogador *jogador_buscar_nick(Campeonato *c, const char *nick) {
    int i = busca_linear_jogador_nick(c->jogadores, c->n_jogadores, nick);
    if (i == -1)
        return NULL;
    return &c->jogadores[i];
}
