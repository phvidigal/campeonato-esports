#ifndef ORDENACAO_H
#define ORDENACAO_H

#include "../models/campeonato.h"

/* Insertion sort: pontos desc, depois saldo desc, depois id asc. */
void insertion_sort_ranking(Equipe *v[], int n);

/* Insertion sort por nick (ordem alfabética). */
void insertion_sort_jogadores_nick(Jogador *v[], int n);

#endif
