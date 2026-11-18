#include <stdio.h>
#include <string.h>
#include "equipe.h"
#include "../algoritmos/busca.h"

/* Insere mantendo equipes[] ordenado por id (mesma ideia do insertion sort). */
int equipe_inserir_ordenado(Campeonato *c, Equipe e) {
    if (c->n_equipes == MAX_EQ)
        return ERR_CHEIO;

    int i = c->n_equipes - 1;
    while (i >= 0 && c->equipes[i].id > e.id) {
        c->equipes[i + 1] = c->equipes[i];   /* empurra para a direita */
        i--;
    }
    c->equipes[i + 1] = e;
    c->n_equipes++;
    return OK;
}

int equipe_cadastrar(Campeonato *c, int id, const char *nome) {
    if (campeonato_fase_atual(c) != FASE_CADASTRO)
        return ERR_BLOQUEADO;
    if (id <= 0 || nome[0] == '\0')
        return ERR_INVALIDO;
    if (busca_binaria_equipe(c->equipes, c->n_equipes, id) != -1)
        return ERR_DUPLICADO;

    Equipe e = {0};   /* pontos, vitórias, derrotas e saldo começam zerados */
    e.id = id;
    strncpy(e.nome, nome, TAM_NOME - 1);
    e.nome[TAM_NOME - 1] = '\0';

    return equipe_inserir_ordenado(c, e);   /* ERR_CHEIO vem daqui */
}

/* Extra: nome vazio também é recusado. Permitido em qualquer fase. */
int equipe_alterar_nome(Campeonato *c, int id, const char *novo_nome) {
    Equipe *e = equipe_buscar(c, id);
    if (e == NULL)
        return ERR_NAO_ENCONTRADO;
    if (novo_nome[0] == '\0')
        return ERR_INVALIDO;

    strncpy(e->nome, novo_nome, TAM_NOME - 1);
    e->nome[TAM_NOME - 1] = '\0';
    return OK;
}

int equipe_excluir(Campeonato *c, int id) {
    if (campeonato_fase_atual(c) != FASE_CADASTRO)
        return ERR_BLOQUEADO;

    int pos = busca_binaria_equipe(c->equipes, c->n_equipes, id);
    if (pos == -1)
        return ERR_NAO_ENCONTRADO;

    /* Remove os jogadores dela de trás para frente: o deslocamento
       à esquerda só mexe na parte que já foi verificada. */
    for (int j = c->n_jogadores - 1; j >= 0; j--) {
        if (c->jogadores[j].id_equipe == id) {
            for (int k = j; k < c->n_jogadores - 1; k++)
                c->jogadores[k] = c->jogadores[k + 1];
            c->n_jogadores--;
        }
    }

    /* Desloca à esquerda para manter equipes[] ordenado por id. */
    for (int k = pos; k < c->n_equipes - 1; k++)
        c->equipes[k] = c->equipes[k + 1];
    c->n_equipes--;
    return OK;
}

/* Devolve o endereço da equipe dentro do array (ou NULL). */
Equipe *equipe_buscar(Campeonato *c, int id) {
    int i = busca_binaria_equipe(c->equipes, c->n_equipes, id);
    if (i == -1)
        return NULL;
    return &c->equipes[i];
}

void equipe_listar(const Campeonato *c) {
    int indices[MAX_JOG];

    if (c->n_equipes == 0) {
        printf("\nNenhuma equipe cadastrada.\n");
        return;
    }

    printf("\n--- EQUIPES (%d/%d) ---\n", c->n_equipes, MAX_EQ);
    printf("Id    %-30s Jogadores\n", "Nome");
    for (int i = 0; i < c->n_equipes; i++) {
        int qtd = busca_linear_jogadores_equipe(c->jogadores, c->n_jogadores,
                                                c->equipes[i].id, indices);
        printf("%-5d %-30s %d/%d\n", c->equipes[i].id, c->equipes[i].nome, qtd, JOG_POR_EQ);
    }
}
