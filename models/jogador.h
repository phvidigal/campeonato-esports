#ifndef JOGADOR_H
#define JOGADOR_H

#include "campeonato.h"

int  jogador_cadastrar(Campeonato *c, const char *nick, const char *nome, int id_equipe, int *id_gerado);
int  jogador_alterar(Campeonato *c, int id, const char *novo_nome, int novo_id_equipe);
int  jogador_excluir(Campeonato *c, int id);
void jogador_listar_por_nick(const Campeonato *c);      /* usa insertion_sort_jogadores_nick */
void jogador_listar_da_equipe(const Campeonato *c, int id_equipe);
Jogador *jogador_buscar_nick(Campeonato *c, const char *nick);

#endif
