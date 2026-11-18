#include <stdio.h>
#include "partida.h"
#include "../algoritmos/busca.h"
#include "../algoritmos/ordenacao.h"

/* ================= Funções de apoio (internas) ================= */

/* Cria uma partida nova no fim do array. */
static int nova_partida(Campeonato *c, int fase, int rodada, int id_a, int id_b) {
    if (c->n_partidas >= MAX_PART)
        return ERR_CHEIO;

    Partida *p = &c->partidas[c->n_partidas];
    p->id = c->prox_id_partida++;
    p->fase = fase;
    p->rodada = rodada;
    p->id_a = id_a;
    p->id_b = id_b;
    p->placar_a = 0;
    p->placar_b = 0;
    p->finalizada = 0;
    c->n_partidas++;
    return OK;
}

/* Devolve o nome da equipe pelo id (busca binária). */
static const char *nome_da_equipe(const Campeonato *c, int id) {
    int i = busca_binaria_equipe(c->equipes, c->n_equipes, id);
    return (i == -1) ? "?" : c->equipes[i].nome;
}

/* Monta um array de ponteiros para as equipes e ordena pelo ranking.
   O array original (c->equipes) não muda de ordem. */
static void montar_ranking(Campeonato *c, Equipe *r[]) {
    for (int i = 0; i < c->n_equipes; i++)
        r[i] = &c->equipes[i];
    insertion_sort_ranking(r, c->n_equipes);
}

/* ================= Pareamentos ================= */

/* Rodada 1: equipes em ordem de id -> 0x4, 1x5, 2x6, 3x7. */
static void parear_rodada1(Campeonato *c) {
    for (int i = 0; i < c->n_equipes / 2; i++)
        nova_partida(c, FASE_SUICO, 1,
                     c->equipes[i].id,
                     c->equipes[i + c->n_equipes / 2].id);
}

/* Rodadas 2 e 3: ordena pelo ranking e pareia vizinhos sem revanche. */
static void parear_suico(Campeonato *c, int rodada) {
    Equipe *r[MAX_EQ];
    int usado[MAX_EQ] = {0};

    montar_ranking(c, r);

    for (int i = 0; i < c->n_equipes; i++) {
        if (usado[i]) continue;

        int par = -1;
        for (int j = i + 1; j < c->n_equipes; j++) {
            int ia = r[i] - c->equipes;   /* aritmética de ponteiros = índice na matriz */
            int ib = r[j] - c->equipes;
            if (!usado[j] && c->confronto[ia][ib] == 0) {
                par = j;
                break;
            }
        }

        /* Travou: só sobraram times que já se enfrentaram. Aceita revanche. */
        if (par == -1) {
            for (int j = i + 1; j < c->n_equipes; j++) {
                if (!usado[j]) { par = j; break; }
            }
        }

        if (par == -1) break; /* não deveria acontecer com número par de equipes */

        nova_partida(c, FASE_SUICO, rodada, r[i]->id, r[par]->id);
        usado[i] = 1;
        usado[par] = 1;
    }
}

/* Semifinal: 1º x 4º e 2º x 3º do ranking. */
static void gerar_semifinais(Campeonato *c) {
    Equipe *r[MAX_EQ];
    montar_ranking(c, r);
    nova_partida(c, FASE_SEMI, 0, r[0]->id, r[3]->id);
    nova_partida(c, FASE_SEMI, 0, r[1]->id, r[2]->id);
}

/* Final: vencedores das duas semifinais (as 2 últimas partidas). */
static void gerar_final(Campeonato *c) {
    int v1 = vencedor_partida(&c->partidas[c->n_partidas - 2]);
    int v2 = vencedor_partida(&c->partidas[c->n_partidas - 1]);
    nova_partida(c, FASE_FINAL, 0, v1, v2);
}

/* ================= Funções públicas ================= */

int vencedor_partida(const Partida *p) {
    return (p->placar_a > p->placar_b) ? p->id_a : p->id_b;
}

/* Reconstrói pontos, saldo e matriz do zero a partir das partidas.
   Por isso nada disso precisa ir para o banco. */
void recalcular_classificacao(Campeonato *c) {
    /* 1. zera tudo: pontos das equipes e a matriz inteira */
    for (int i = 0; i < c->n_equipes; i++) {
        c->equipes[i].pontos = 0;
        c->equipes[i].vitorias = 0;
        c->equipes[i].derrotas = 0;
        c->equipes[i].saldo = 0;
    }
    for (int i = 0; i < MAX_EQ; i++)
        for (int j = 0; j < MAX_EQ; j++)
            c->confronto[i][j] = 0;

    /* 2. reprocessa cada partida que já tem resultado */
    for (int k = 0; k < c->n_partidas; k++) {
        const Partida *p = &c->partidas[k];
        if (!p->finalizada) continue;

        int ia = busca_binaria_equipe(c->equipes, c->n_equipes, p->id_a);
        int ib = busca_binaria_equipe(c->equipes, c->n_equipes, p->id_b);
        if (ia == -1 || ib == -1) continue;

        /* quem ganhou e quem perdeu */
        int ganhador, perdedor, rounds_g, rounds_p;
        if (p->placar_a > p->placar_b) {
            ganhador = ia; perdedor = ib;
            rounds_g = p->placar_a; rounds_p = p->placar_b;
        } else {
            ganhador = ib; perdedor = ia;
            rounds_g = p->placar_b; rounds_p = p->placar_a;
        }

        /* 3. matriz vale para qualquer fase */
        c->confronto[ganhador][perdedor] = 1;
        c->confronto[perdedor][ganhador] = -1;

        /* 4. pontos e saldo só contam no suíço */
        if (p->fase == FASE_SUICO) {
            c->equipes[ganhador].pontos += 3;
            c->equipes[ganhador].vitorias++;
            c->equipes[perdedor].derrotas++;
            c->equipes[ganhador].saldo += rounds_g - rounds_p;
            c->equipes[perdedor].saldo += rounds_p - rounds_g;
        }
    }
}

int partida_registrar_resultado(Campeonato *c, int id_partida, int placar_a, int placar_b) {
    int i = busca_linear_partida(c->partidas, c->n_partidas, id_partida);
    if (i == -1)
        return ERR_NAO_ENCONTRADO;

    if (placar_a < 0 || placar_b < 0 || placar_a == placar_b)
        return ERR_INVALIDO;

    /* Só dá para alterar a rodada atual (a da última partida criada). */
    const Partida *ultima = &c->partidas[c->n_partidas - 1];
    Partida *p = &c->partidas[i];
    if (p->fase != ultima->fase || p->rodada != ultima->rodada)
        return ERR_BLOQUEADO;

    p->placar_a = placar_a;
    p->placar_b = placar_b;
    p->finalizada = 1;

    recalcular_classificacao(c);
    return OK;
}

int gerar_proxima_rodada(Campeonato *c, int *qtd_novas) {
    int antes = c->n_partidas;
    *qtd_novas = 0;

    int fase = campeonato_fase_atual(c);

    if (fase == FASE_ENCERRADO)
        return ERR_BLOQUEADO;
    if (fase != FASE_CADASTRO && !campeonato_rodada_concluida(c))
        return ERR_PENDENTE;

    recalcular_classificacao(c); /* garante ranking em dia */

    if (fase == FASE_CADASTRO) {
        if (c->n_equipes != MAX_EQ)
            return ERR_INVALIDO;
        parear_rodada1(c);
    } else if (fase == FASE_SUICO) {
        int rodada = campeonato_rodada_atual(c);
        if (rodada < RODADAS_SUICO)
            parear_suico(c, rodada + 1);
        else
            gerar_semifinais(c);
    } else if (fase == FASE_SEMI) {
        gerar_final(c);
    }

    *qtd_novas = c->n_partidas - antes; /* o main grava essas partidas no banco */
    return OK;
}

/* ================= Telas ================= */

void exibir_ranking(const Campeonato *c) {
    Equipe *r[MAX_EQ];
    /* o ranking mexe só no array de ponteiros; o cast é só porque c é const */
    montar_ranking((Campeonato *)c, r);

    printf("\n--- RANKING ---\n");
    printf("Pos  Id   %-20s Pts  V  D  Saldo\n", "Equipe");
    for (int i = 0; i < c->n_equipes; i++) {
        printf("%2d   %-4d %-20s %3d %2d %2d  %+4d\n",
               i + 1, r[i]->id, r[i]->nome,
               r[i]->pontos, r[i]->vitorias, r[i]->derrotas, r[i]->saldo);
    }
}

void exibir_matriz_confrontos(const Campeonato *c) {
    printf("\n--- MATRIZ DE CONFRONTOS ---\n");
    printf("(V = linha venceu a coluna, D = linha perdeu, - = não jogaram)\n\n");

    printf("     ");
    for (int j = 0; j < c->n_equipes; j++)
        printf("%4d", c->equipes[j].id);
    printf("\n");

    for (int i = 0; i < c->n_equipes; i++) {
        printf("%4d ", c->equipes[i].id);
        for (int j = 0; j < c->n_equipes; j++) {
            char simbolo = '-';
            if (i == j)                       simbolo = 'x';
            else if (c->confronto[i][j] == 1)  simbolo = 'V';
            else if (c->confronto[i][j] == -1) simbolo = 'D';
            printf("%4c", simbolo);
        }
        printf("\n");
    }
}

void listar_partidas_rodada_atual(const Campeonato *c) {
    if (c->n_partidas == 0) {
        printf("\nNenhuma partida gerada ainda.\n");
        return;
    }

    int fase = c->partidas[c->n_partidas - 1].fase;
    int rodada = c->partidas[c->n_partidas - 1].rodada;

    printf("\n--- PARTIDAS DA RODADA ATUAL ---\n");
    for (int i = 0; i < c->n_partidas; i++) {
        const Partida *p = &c->partidas[i];
        if (p->fase != fase || p->rodada != rodada) continue;

        printf("Partida %d: %s x %s  ", p->id,
               nome_da_equipe(c, p->id_a), nome_da_equipe(c, p->id_b));
        if (p->finalizada)
            printf("[%d x %d]\n", p->placar_a, p->placar_b);
        else
            printf("[aguardando resultado]\n");
    }
}

void exibir_campeao(const Campeonato *c) {
    if (campeonato_fase_atual(c) != FASE_ENCERRADO) {
        printf("\nO campeonato ainda não terminou.\n");
        return;
    }
    const Partida *final = &c->partidas[c->n_partidas - 1];
    printf("\n*** CAMPEÃO: %s ***\n", nome_da_equipe(c, vencedor_partida(final)));
}
