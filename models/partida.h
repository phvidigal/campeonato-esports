#ifndef PARTIDA_H
#define PARTIDA_H

#include "campeonato.h"

void recalcular_classificacao(Campeonato *c);   /* zera pontos/saldo/matriz e reprocessa as partidas finalizadas */
int partida_registrar_resultado(Campeonato *c, int id_partida, int placar_a, int placar_b);
int gerar_proxima_rodada(Campeonato *c, int *qtd_novas); /* gera rodada 1, 2, 3, semi ou final */
int vencedor_partida(const Partida *p);         /* id do vencedor */
void exibir_ranking(const Campeonato *c);
void exibir_matriz_confrontos(const Campeonato *c);
void listar_partidas_rodada_atual(const Campeonato *c);
void exibir_campeao(const Campeonato *c);

#endif
