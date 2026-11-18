#ifndef EQUIPE_H
#define EQUIPE_H

#include "campeonato.h"

int  equipe_inserir_ordenado(Campeonato *c, Equipe e);  /* desloca à direita e insere na posição do id. Também usada no carregamento */
int  equipe_cadastrar(Campeonato *c, int id, const char *nome);
int  equipe_alterar_nome(Campeonato *c, int id, const char *novo_nome);
int  equipe_excluir(Campeonato *c, int id);             /* remove os jogadores dela (de trás para frente) e desloca à esquerda */
Equipe *equipe_buscar(Campeonato *c, int id);           /* ponteiro ou NULL (usa busca binária) */
void equipe_listar(const Campeonato *c);                /* ordem de id, com qtd de jogadores */

#endif
