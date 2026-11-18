#include <ctype.h>
#include "ordenacao.h"

/* Diz se a equipe "a" fica na frente da equipe "b" no ranking.
   Desempate: 1º pontos, 2º saldo, 3º menor id. */
static int vem_antes(const Equipe *a, const Equipe *b) {
    if (a->pontos != b->pontos) return a->pontos > b->pontos;
    if (a->saldo != b->saldo)   return a->saldo > b->saldo;
    return a->id < b->id;
}

/* Compara dois nicks ignorando maiúsculas/minúsculas (como strcmp). */
static int comparar_nick(const char *a, const char *b) {
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* Os dois sorts trabalham com arrays de PONTEIROS: o array original
   (c->equipes) continua ordenado por id e a busca binária segue funcionando. */
void insertion_sort_ranking(Equipe *v[], int n) {
    for (int i = 1; i < n; i++) {
        Equipe *chave = v[i];
        int j = i - 1;
        while (j >= 0 && vem_antes(chave, v[j])) {
            v[j + 1] = v[j];   /* empurra para a direita */
            j--;
        }
        v[j + 1] = chave;
    }
}

void insertion_sort_jogadores_nick(Jogador *v[], int n) {
    for (int i = 1; i < n; i++) {
        Jogador *chave = v[i];
        int j = i - 1;
        while (j >= 0 && comparar_nick(chave->nick, v[j]->nick) < 0) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = chave;
    }
}
