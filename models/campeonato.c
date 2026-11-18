#include <string.h>
#include "campeonato.h"

void campeonato_init(Campeonato *c) {
    memset(c, 0, sizeof(Campeonato)); /* zera arrays e contadores */
    c->prox_id_jogador = 1;
    c->prox_id_partida = 1;
}

/* A fase nunca é salva: a gente descobre olhando a última partida criada. */
int campeonato_fase_atual(const Campeonato *c) {
    if (c->n_partidas == 0)
        return FASE_CADASTRO;

    const Partida *ultima = &c->partidas[c->n_partidas - 1];

    if (ultima->fase == FASE_FINAL && ultima->finalizada)
        return FASE_ENCERRADO;

    return ultima->fase;
}

/* A rodada atual é a rodada da última partida (0 se ainda não há partidas). */
int campeonato_rodada_atual(const Campeonato *c) {
    if (c->n_partidas == 0)
        return 0;
    return c->partidas[c->n_partidas - 1].rodada;
}

/* A rodada atual é o par (fase, rodada) da última partida.
   Está concluída quando todas as partidas desse par têm resultado. */
int campeonato_rodada_concluida(const Campeonato *c) {
    if (c->n_partidas == 0)
        return 1;

    int fase = c->partidas[c->n_partidas - 1].fase;
    int rodada = c->partidas[c->n_partidas - 1].rodada;

    for (int i = 0; i < c->n_partidas; i++) {
        const Partida *p = &c->partidas[i];
        if (p->fase == fase && p->rodada == rodada && !p->finalizada)
            return 0;
    }
    return 1;
}

const char *msg_erro(int codigo) {
    switch (codigo) {
        case OK:                return "Sem erro.";
        case ERR_DUPLICADO:     return "Esse registro já existe.";
        case ERR_NAO_ENCONTRADO:return "Registro não encontrado.";
        case ERR_CHEIO:         return "Limite atingido (equipe ou lista cheia).";
        case ERR_INVALIDO:      return "Dados inválidos.";
        case ERR_BLOQUEADO:     return "Operação não permitida na fase atual.";
        case ERR_PENDENTE:      return "Ainda há partidas da rodada atual sem resultado.";
        default:                return "Erro desconhecido.";
    }
}
