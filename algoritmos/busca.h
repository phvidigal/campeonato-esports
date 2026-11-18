#ifndef BUSCA_H
#define BUSCA_H

#include "../models/campeonato.h"

/* Retorna o índice da equipe ou -1. Exige v[] ordenado por id. */
int busca_binaria_equipe(const Equipe v[], int n, int id);
/* Retorna o índice ou -1. Comparação sem diferenciar maiúsculas. */
int busca_linear_jogador_nick(const Jogador v[], int n, const char *nick);
/* Preenche indices[] com as posições dos jogadores da equipe. Retorna quantos achou. */
int busca_linear_jogadores_equipe(const Jogador v[], int n, int id_equipe, int indices[]);
/* Retorna o índice ou -1. */
int busca_linear_partida(const Partida v[], int n, int id);
/* Retorna o índice ou -1. Extensão combinada entre A e B (jogador_alterar/excluir recebem id). */
int busca_linear_jogador_id(const Jogador v[], int n, int id);

#endif
