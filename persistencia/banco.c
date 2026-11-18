#include <stdio.h>
#include <string.h>
#include "banco.h"
#include "../models/equipe.h"

/* Mesmo SQL do schema.sql (os dois precisam ficar iguais). */
static const char SQL_SCHEMA[] =
    "CREATE TABLE IF NOT EXISTS equipes (\n"
    "    id   INTEGER PRIMARY KEY,\n"
    "    nome TEXT    NOT NULL\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS jogadores (\n"
    "    id        INTEGER PRIMARY KEY,\n"
    "    nick      TEXT    NOT NULL UNIQUE,\n"
    "    nome      TEXT    NOT NULL,\n"
    "    id_equipe INTEGER NOT NULL REFERENCES equipes(id) ON DELETE CASCADE\n"
    ");\n"
    "\n"
    "CREATE TABLE IF NOT EXISTS partidas (\n"
    "    id         INTEGER PRIMARY KEY,\n"
    "    fase       INTEGER NOT NULL CHECK (fase BETWEEN 1 AND 3),\n"
    "    rodada     INTEGER NOT NULL,\n"
    "    id_a       INTEGER NOT NULL REFERENCES equipes(id),\n"
    "    id_b       INTEGER NOT NULL REFERENCES equipes(id),\n"
    "    placar_a   INTEGER NOT NULL DEFAULT 0,\n"
    "    placar_b   INTEGER NOT NULL DEFAULT 0,\n"
    "    finalizada INTEGER NOT NULL DEFAULT 0\n"
    ");\n";

/* ================= Funções de apoio (internas) ================= */

/* Compila um comando SQL com '?' no lugar dos valores. */
static int preparar(sqlite3 *db, const char *sql, sqlite3_stmt **stmt) {
    if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK) {
        printf("Erro no banco: %s\n", sqlite3_errmsg(db));
        return ERR_BANCO;
    }
    return OK;
}

/* Executa um INSERT/UPDATE/DELETE já com os binds feitos. Sempre finaliza. */
static int executar(sqlite3 *db, sqlite3_stmt *stmt) {
    int r = OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        printf("Erro no banco: %s\n", sqlite3_errmsg(db));
        r = ERR_BANCO;
    }
    sqlite3_finalize(stmt);
    return r;
}

/* Copia uma coluna de texto para o array, sempre com '\0' no fim. */
static void copiar_texto(char *dest, const unsigned char *origem, int tam) {
    if (origem == NULL) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, (const char *)origem, tam - 1);
    dest[tam - 1] = '\0';
}

/* ================= Conexão ================= */

/* sqlite3 **db: a função precisa mudar o ponteiro que está no main. */
int banco_abrir(sqlite3 **db, const char *arquivo) {
    sqlite3_stmt *stmt;
    char *erro = NULL;

    if (sqlite3_open(arquivo, db) != SQLITE_OK) {
        printf("Erro ao abrir o banco: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        *db = NULL;
        return ERR_BANCO;
    }

    /* Sem isso o SQLite ignora o ON DELETE CASCADE. */
    if (preparar(*db, "PRAGMA foreign_keys = ON", &stmt) != OK ||
        executar(*db, stmt) != OK)
        return ERR_BANCO;

    /* Schema com sqlite3_exec: são 3 comandos e o prepare_v2 só compila um. */
    if (sqlite3_exec(*db, SQL_SCHEMA, NULL, NULL, &erro) != SQLITE_OK) {
        printf("Erro ao criar as tabelas: %s\n", erro);
        sqlite3_free(erro);
        return ERR_BANCO;
    }
    return OK;
}

void banco_fechar(sqlite3 *db) {
    sqlite3_close(db);
}

/* ================= Carregamento ================= */

/* Sem WHERE e sem ORDER BY: quem ordena as equipes é o C. */
static int carregar_equipes(sqlite3 *db, Campeonato *c) {
    sqlite3_stmt *stmt;
    int rc;

    if (preparar(db, "SELECT id, nome FROM equipes", &stmt) != OK)
        return ERR_BANCO;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Equipe e = {0};   /* campos calculados são refeitos pelo recalcular_classificacao */
        e.id = sqlite3_column_int(stmt, 0);
        copiar_texto(e.nome, sqlite3_column_text(stmt, 1), TAM_NOME);
        if (equipe_inserir_ordenado(c, e) != OK) {
            printf("Aviso: equipe %d ignorada (limite de %d).\n", e.id, MAX_EQ);
        }
    }
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? OK : ERR_BANCO;
}

static int carregar_jogadores(sqlite3 *db, Campeonato *c) {
    sqlite3_stmt *stmt;
    int rc, maior_id = 0;

    if (preparar(db, "SELECT id, nick, nome, id_equipe FROM jogadores", &stmt) != OK)
        return ERR_BANCO;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (c->n_jogadores == MAX_JOG) {
            printf("Aviso: limite de %d jogadores atingido no carregamento.\n", MAX_JOG);
            rc = SQLITE_DONE;
            break;
        }
        Jogador *j = &c->jogadores[c->n_jogadores];   /* acrescenta no fim */
        j->id = sqlite3_column_int(stmt, 0);
        copiar_texto(j->nick, sqlite3_column_text(stmt, 1), TAM_NICK);
        copiar_texto(j->nome, sqlite3_column_text(stmt, 2), TAM_NOME);
        j->id_equipe = sqlite3_column_int(stmt, 3);
        c->n_jogadores++;
        if (j->id > maior_id)
            maior_id = j->id;
    }
    sqlite3_finalize(stmt);
    c->prox_id_jogador = maior_id + 1;   /* maior + 1: ids excluídos não voltam */
    return (rc == SQLITE_DONE) ? OK : ERR_BANCO;
}

static int carregar_partidas(sqlite3 *db, Campeonato *c) {
    sqlite3_stmt *stmt;
    int rc, maior_id = 0;

    if (preparar(db, "SELECT id, fase, rodada, id_a, id_b, placar_a, placar_b, finalizada "
                     "FROM partidas", &stmt) != OK)
        return ERR_BANCO;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (c->n_partidas == MAX_PART) {
            printf("Aviso: limite de %d partidas atingido no carregamento.\n", MAX_PART);
            rc = SQLITE_DONE;
            break;
        }
        Partida *p = &c->partidas[c->n_partidas];
        p->id = sqlite3_column_int(stmt, 0);
        p->fase = sqlite3_column_int(stmt, 1);
        p->rodada = sqlite3_column_int(stmt, 2);
        p->id_a = sqlite3_column_int(stmt, 3);
        p->id_b = sqlite3_column_int(stmt, 4);
        p->placar_a = sqlite3_column_int(stmt, 5);
        p->placar_b = sqlite3_column_int(stmt, 6);
        p->finalizada = sqlite3_column_int(stmt, 7);
        c->n_partidas++;
        if (p->id > maior_id)
            maior_id = p->id;
    }
    sqlite3_finalize(stmt);
    c->prox_id_partida = maior_id + 1;
    return (rc == SQLITE_DONE) ? OK : ERR_BANCO;
}

/* Lê as 3 tabelas. Pontos, saldo e matriz o main recalcula depois. */
int banco_carregar(sqlite3 *db, Campeonato *c) {
    if (carregar_equipes(db, c) != OK ||
        carregar_jogadores(db, c) != OK ||
        carregar_partidas(db, c) != OK) {
        printf("Erro ao carregar: %s\n", sqlite3_errmsg(db));
        return ERR_BANCO;
    }
    return OK;
}

/* ================= Equipes ================= */

int banco_inserir_equipe(sqlite3 *db, const Equipe *e) {
    sqlite3_stmt *stmt;
    if (preparar(db, "INSERT INTO equipes (id, nome) VALUES (?, ?)", &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, e->id);
    sqlite3_bind_text(stmt, 2, e->nome, -1, SQLITE_TRANSIENT);
    return executar(db, stmt);
}

/* WHERE id = ? só diz qual linha gravar: a busca já foi feita em C. */
int banco_atualizar_equipe(sqlite3 *db, const Equipe *e) {
    sqlite3_stmt *stmt;
    if (preparar(db, "UPDATE equipes SET nome = ? WHERE id = ?", &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_text(stmt, 1, e->nome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, e->id);
    return executar(db, stmt);
}

/* Os jogadores da equipe saem sozinhos pelo ON DELETE CASCADE. */
int banco_excluir_equipe(sqlite3 *db, int id) {
    sqlite3_stmt *stmt;
    if (preparar(db, "DELETE FROM equipes WHERE id = ?", &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, id);
    return executar(db, stmt);
}

/* ================= Jogadores ================= */

int banco_inserir_jogador(sqlite3 *db, const Jogador *j) {
    sqlite3_stmt *stmt;
    if (preparar(db, "INSERT INTO jogadores (id, nick, nome, id_equipe) VALUES (?, ?, ?, ?)",
                 &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, j->id);
    sqlite3_bind_text(stmt, 2, j->nick, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, j->nome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, j->id_equipe);
    return executar(db, stmt);
}

int banco_atualizar_jogador(sqlite3 *db, const Jogador *j) {
    sqlite3_stmt *stmt;
    if (preparar(db, "UPDATE jogadores SET nick = ?, nome = ?, id_equipe = ? WHERE id = ?",
                 &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_text(stmt, 1, j->nick, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, j->nome, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, j->id_equipe);
    sqlite3_bind_int(stmt, 4, j->id);
    return executar(db, stmt);
}

int banco_excluir_jogador(sqlite3 *db, int id) {
    sqlite3_stmt *stmt;
    if (preparar(db, "DELETE FROM jogadores WHERE id = ?", &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, id);
    return executar(db, stmt);
}

/* ================= Partidas ================= */

int banco_inserir_partida(sqlite3 *db, const Partida *p) {
    sqlite3_stmt *stmt;
    if (preparar(db, "INSERT INTO partidas (id, fase, rodada, id_a, id_b, placar_a, placar_b, finalizada) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)", &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, p->id);
    sqlite3_bind_int(stmt, 2, p->fase);
    sqlite3_bind_int(stmt, 3, p->rodada);
    sqlite3_bind_int(stmt, 4, p->id_a);
    sqlite3_bind_int(stmt, 5, p->id_b);
    sqlite3_bind_int(stmt, 6, p->placar_a);
    sqlite3_bind_int(stmt, 7, p->placar_b);
    sqlite3_bind_int(stmt, 8, p->finalizada);
    return executar(db, stmt);
}

int banco_atualizar_partida(sqlite3 *db, const Partida *p) {
    sqlite3_stmt *stmt;
    if (preparar(db, "UPDATE partidas SET placar_a = ?, placar_b = ?, finalizada = ? WHERE id = ?",
                 &stmt) != OK)
        return ERR_BANCO;
    sqlite3_bind_int(stmt, 1, p->placar_a);
    sqlite3_bind_int(stmt, 2, p->placar_b);
    sqlite3_bind_int(stmt, 3, p->finalizada);
    sqlite3_bind_int(stmt, 4, p->id);
    return executar(db, stmt);
}
