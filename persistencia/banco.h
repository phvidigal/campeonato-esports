#ifndef BANCO_H
#define BANCO_H

#include "sqlite3.h"
#include "../models/campeonato.h"

/* Falha do SQLite (a mensagem do sqlite3_errmsg já é impressa pelo banco.c). */
#define ERR_BANCO 7

int  banco_abrir(sqlite3 **db, const char *arquivo);  /* abre, PRAGMA foreign_keys = ON, CREATE TABLE IF NOT EXISTS */
void banco_fechar(sqlite3 *db);
int  banco_carregar(sqlite3 *db, Campeonato *c);       /* SELECT das 3 tabelas para os arrays */
int  banco_inserir_equipe(sqlite3 *db, const Equipe *e);
int  banco_atualizar_equipe(sqlite3 *db, const Equipe *e);
int  banco_excluir_equipe(sqlite3 *db, int id);        /* jogadores saem por ON DELETE CASCADE */
int  banco_inserir_jogador(sqlite3 *db, const Jogador *j);
int  banco_atualizar_jogador(sqlite3 *db, const Jogador *j);
int  banco_excluir_jogador(sqlite3 *db, int id);
int  banco_inserir_partida(sqlite3 *db, const Partida *p);
int  banco_atualizar_partida(sqlite3 *db, const Partida *p);

#endif
