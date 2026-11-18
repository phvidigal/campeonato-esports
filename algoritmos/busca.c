#include <ctype.h>
#include "busca.h"

/* Compara dois nicks sem diferenciar maiúsculas. Retorna 1 se iguais. */
static int nick_igual(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    /* só são iguais se as duas terminaram juntas */
    return *a == '\0' && *b == '\0';
}

/* Retorna o índice da equipe ou -1. Exige v[] ordenado por id. */
int busca_binaria_equipe(const Equipe v[], int n, int id) {
    int inicio = 0;
    int fim = n - 1;

    while (inicio <= fim) {
        int meio = inicio + (fim - inicio) / 2;

        if (v[meio].id == id) {
            return meio;
        } else if (v[meio].id < id) {
            inicio = meio + 1;   /* descarta a metade da esquerda */
        } else {
            fim = meio - 1;      /* descarta a metade da direita */
        }
    }

    return -1;
}

/* Retorna o índice ou -1. Comparação sem diferenciar maiúsculas. */
int busca_linear_jogador_nick(const Jogador v[], int n, const char *nick) {
    for (int i = 0; i < n; i++) {
        if (nick_igual(v[i].nick, nick))
            return i;
    }
    return -1;
}

/* Preenche indices[] com as posições dos jogadores da equipe. Retorna quantos achou. */
int busca_linear_jogadores_equipe(const Jogador v[], int n, int id_equipe, int indices[]) {
    int qtd = 0;
    for (int i = 0; i < n; i++) {
        if (v[i].id_equipe == id_equipe) {
            indices[qtd] = i;
            qtd++;
        }
    }
    return qtd;
}

/* Retorna o índice ou -1. */
int busca_linear_partida(const Partida v[], int n, int id) {
    for (int i = 0; i < n; i++) {
        if (v[i].id == id)
            return i;
    }
    return -1;
}

/* Retorna o índice do jogador com esse id ou -1. */
int busca_linear_jogador_id(const Jogador v[], int n, int id) {
    for (int i = 0; i < n; i++) {
        if (v[i].id == id)
            return i;
    }
    return -1;
}
