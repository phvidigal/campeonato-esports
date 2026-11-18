/* ============================================================
   SERVIDOR WEB DO CAMPEONATO (frontend)

   É um segundo programa, ao lado do main.c. Ele usa os MESMOS models,
   algoritmos e banco: o navegador só mostra os dados e manda os comandos,
   e toda regra (código repetido, equipe cheia, empate...) continua no C.

   Como funciona:
   1. Abre o esports.db e carrega tudo para o Campeonato (igual ao main).
   2. Espera conexões em http://127.0.0.1:8080.
   3. GET  /             -> manda os arquivos da pasta web/ (HTML, CSS, JS)
      GET  /api/...      -> devolve dados em JSON
      POST /api/...      -> chama o model; se deu OK, grava no banco
   O padrão "model valida, depois banco grava" é o mesmo do main.c.
   ============================================================ */

#ifdef _WIN32
#include <winsock2.h>   /* sockets do Windows (link com -lws2_32) */
typedef SOCKET sock_t;
#define fechar_socket closesocket
#else
#define _POSIX_C_SOURCE 200112L
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define fechar_socket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "../models/campeonato.h"
#include "../models/equipe.h"
#include "../models/jogador.h"
#include "../models/partida.h"
#include "../algoritmos/busca.h"
#include "../algoritmos/ordenacao.h"
#include "../persistencia/banco.h"

#define PORTA_PADRAO 8080
#define TAM_REQ 16384      /* maior pedido aceito (cabeçalho + corpo) */
#define TAM_RESP 65536     /* maior resposta JSON */

/* Estado do programa: o mesmo par (Campeonato, sqlite3*) que o main usa. */
static Campeonato camp;
static sqlite3 *db = NULL;

/* ============================================================
   MONTAGEM DO JSON
   A resposta é montada num buffer fixo, pedaço por pedaço.
   ============================================================ */

static char resp[TAM_RESP];
static size_t resp_n;

/* Igual ao printf, mas escreve no fim do buffer da resposta. */
static void escrever(const char *fmt, ...) {
    if (resp_n >= TAM_RESP - 1)
        return;
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(resp + resp_n, TAM_RESP - resp_n, fmt, ap);
    va_end(ap);
    if (k > 0) {
        resp_n += (size_t)k;
        if (resp_n > TAM_RESP - 1)
            resp_n = TAM_RESP - 1;   /* cortou: não passa do fim */
    }
}

/* Escreve um texto entre aspas, escapando o que o JSON exige. */
static void escrever_str(const char *s) {
    escrever("\"");
    for (; *s != '\0'; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\')
            escrever("\\%c", ch);
        else if (ch < 0x20)
            escrever("\\u%04x", ch);
        else
            escrever("%c", ch);   /* acentos (UTF-8) passam como estão */
    }
    escrever("\"");
}

static void json_equipe(const Campeonato *c, const Equipe *e) {
    int indices[MAX_JOG];
    int qtd = busca_linear_jogadores_equipe(c->jogadores, c->n_jogadores, e->id, indices);

    escrever("{\"id\":%d,\"nome\":", e->id);
    escrever_str(e->nome);
    escrever(",\"pontos\":%d,\"vitorias\":%d,\"derrotas\":%d,\"saldo\":%d,\"jogadores\":%d}",
             e->pontos, e->vitorias, e->derrotas, e->saldo, qtd);
}

static void json_jogador(const Jogador *j) {
    escrever("{\"id\":%d,\"nick\":", j->id);
    escrever_str(j->nick);
    escrever(",\"nome\":");
    escrever_str(j->nome);
    escrever(",\"equipe\":%d}", j->id_equipe);
}

/* Tudo o que a tela precisa de uma vez só. */
static void json_estado(Campeonato *c) {
    int fase = campeonato_fase_atual(c);

    escrever("{\"fase\":%d,\"rodada\":%d,\"rodada_concluida\":%d,",
             fase, campeonato_rodada_atual(c), campeonato_rodada_concluida(c));
    escrever("\"max_equipes\":%d,\"jogadores_por_equipe\":%d,", MAX_EQ, JOG_POR_EQ);

    /* equipes na ordem do array (por código) */
    escrever("\"equipes\":[");
    for (int i = 0; i < c->n_equipes; i++) {
        if (i > 0) escrever(",");
        json_equipe(c, &c->equipes[i]);
    }
    escrever("],");

    /* ranking: mesmo insertion sort de ponteiros do exibir_ranking */
    Equipe *r[MAX_EQ];
    for (int i = 0; i < c->n_equipes; i++)
        r[i] = &c->equipes[i];
    insertion_sort_ranking(r, c->n_equipes);
    escrever("\"ranking\":[");
    for (int i = 0; i < c->n_equipes; i++)
        escrever("%s%d", (i > 0) ? "," : "", r[i]->id);
    escrever("],");

    /* jogadores em ordem de nick: mesmo insertion sort do jogador_listar_por_nick */
    Jogador *v[MAX_JOG];
    for (int i = 0; i < c->n_jogadores; i++)
        v[i] = &c->jogadores[i];
    insertion_sort_jogadores_nick(v, c->n_jogadores);
    escrever("\"jogadores\":[");
    for (int i = 0; i < c->n_jogadores; i++) {
        if (i > 0) escrever(",");
        json_jogador(v[i]);
    }
    escrever("],");

    /* partidas na ordem de criação */
    escrever("\"partidas\":[");
    for (int i = 0; i < c->n_partidas; i++) {
        const Partida *p = &c->partidas[i];
        escrever("%s{\"id\":%d,\"fase\":%d,\"rodada\":%d,\"a\":%d,\"b\":%d,"
                 "\"placar_a\":%d,\"placar_b\":%d,\"finalizada\":%d}",
                 (i > 0) ? "," : "", p->id, p->fase, p->rodada, p->id_a, p->id_b,
                 p->placar_a, p->placar_b, p->finalizada);
    }
    escrever("],");

    /* matriz de confrontos: linha i, coluna j = posições em equipes[] */
    escrever("\"matriz\":[");
    for (int i = 0; i < c->n_equipes; i++) {
        escrever("%s[", (i > 0) ? "," : "");
        for (int j = 0; j < c->n_equipes; j++)
            escrever("%s%d", (j > 0) ? "," : "", c->confronto[i][j]);
        escrever("]");
    }
    escrever("],");

    if (fase == FASE_ENCERRADO)
        escrever("\"campeao\":%d}", vencedor_partida(&c->partidas[c->n_partidas - 1]));
    else
        escrever("\"campeao\":null}");
}

/* ============================================================
   LEITURA DOS CAMPOS ENVIADOS PELO NAVEGADOR
   O formulário chega como "id=3&nome=Team+Liquid" (form-urlencoded).
   ============================================================ */

static int valor_hex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    ch = (char)tolower((unsigned char)ch);
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

/* Procura "nome=" no texto e copia o valor já decodificado (+ vira espaço,
   %C3%A9 vira "é"). Retorna 1 se achou o campo. */
static int campo(const char *texto, const char *nome, char *dest, int tam) {
    size_t n = strlen(nome);
    const char *p = texto;

    while (p != NULL && *p != '\0') {
        if (strncmp(p, nome, n) == 0 && p[n] == '=') {
            p += n + 1;
            int k = 0;
            while (*p != '\0' && *p != '&' && k < tam - 1) {
                if (*p == '+') {
                    dest[k++] = ' ';
                    p++;
                } else if (*p == '%' && valor_hex(p[1]) >= 0 && valor_hex(p[2]) >= 0) {
                    dest[k++] = (char)(valor_hex(p[1]) * 16 + valor_hex(p[2]));
                    p += 3;
                } else {
                    dest[k++] = *p++;
                }
            }
            dest[k] = '\0';
            return 1;
        }
        p = strchr(p, '&');   /* pula para o próximo campo */
        if (p != NULL) p++;
    }
    return 0;
}

/* Mensagem devolvida ao navegador (sucesso ou erro). */
static char msg[256];

static int falhar(const char *texto) {
    snprintf(msg, sizeof(msg), "%s", texto);
    return 0;
}

/* Se cortou o texto no meio de um caractere acentuado (2 a 4 bytes em UTF-8),
   tira o pedaço que sobrou para não gravar um caractere quebrado. */
static void cortar_utf8(char *s) {
    int n = (int)strlen(s);
    int i = n - 1;
    while (i >= 0 && ((unsigned char)s[i] & 0xC0) == 0x80)
        i--;   /* volta até o primeiro byte do último caractere */
    if (i < 0) return;

    unsigned char ch = (unsigned char)s[i];
    int esperado = (ch >= 0xF0) ? 4 : (ch >= 0xE0) ? 3 : (ch >= 0xC0) ? 2 : 1;
    if (n - i < esperado)
        s[i] = '\0';
}

/* Mesmas regras do ler_texto do main: tira espaços das pontas e recusa vazio. */
static int ler_txt(const char *corpo, const char *nome, char *dest, int tam) {
    char txt[256];
    if (!campo(corpo, nome, txt, sizeof(txt)))
        return falhar("Campo obrigatório não enviado.");

    char *ini = txt;
    while (*ini == ' ' || *ini == '\t') ini++;
    int n = (int)strlen(ini);
    while (n > 0 && (ini[n - 1] == ' ' || ini[n - 1] == '\t' || ini[n - 1] == '\r' || ini[n - 1] == '\n'))
        ini[--n] = '\0';

    if (*ini == '\0')
        return falhar("O texto não pode ser vazio.");

    strncpy(dest, ini, tam - 1);
    dest[tam - 1] = '\0';
    cortar_utf8(dest);
    return 1;
}

/* Mesmas regras do ler_int do main: só número, entre min e max. */
static int ler_num(const char *corpo, const char *nome, int min, int max, int *saida) {
    char txt[32];
    if (!campo(corpo, nome, txt, sizeof(txt)))
        return falhar("Campo obrigatório não enviado.");

    char *fim;
    long valor = strtol(txt, &fim, 10);
    while (*fim == ' ') fim++;

    if (fim == txt || *fim != '\0')
        return falhar("Digite apenas números.");
    if (valor < min || valor > max) {
        snprintf(msg, sizeof(msg), "Digite um valor entre %d e %d.", min, max);
        return 0;
    }
    *saida = (int)valor;
    return 1;
}

/* Fecha um comando que deu certo, avisando se o banco falhou. */
static int concluir(int r_banco, const char *texto) {
    if (r_banco != OK)
        snprintf(msg, sizeof(msg), "%s Aviso: a alteração não foi gravada no banco.", texto);
    else
        snprintf(msg, sizeof(msg), "%s", texto);
    return 1;
}

/* ============================================================
   COMANDOS (POST)
   Cada um é a versão web de uma função do main.c.
   Retornam 1 (deu certo) ou 0 (erro), sempre com o texto em msg.
   ============================================================ */

static int cmd_cadastrar_equipe(const char *corpo) {
    int id;
    char nome[TAM_NOME];
    if (!ler_num(corpo, "id", 1, 9999, &id) || !ler_txt(corpo, "nome", nome, TAM_NOME))
        return 0;

    int r = equipe_cadastrar(&camp, id, nome);
    if (r != OK) return falhar(msg_erro(r));
    return concluir(banco_inserir_equipe(db, equipe_buscar(&camp, id)), "Equipe cadastrada.");
}

static int cmd_alterar_equipe(const char *corpo) {
    int id;
    char nome[TAM_NOME];
    if (!ler_num(corpo, "id", 1, 9999, &id) || !ler_txt(corpo, "nome", nome, TAM_NOME))
        return 0;

    int r = equipe_alterar_nome(&camp, id, nome);
    if (r != OK) return falhar(msg_erro(r));
    return concluir(banco_atualizar_equipe(db, equipe_buscar(&camp, id)), "Nome alterado.");
}

static int cmd_excluir_equipe(const char *corpo) {
    int id;
    if (!ler_num(corpo, "id", 1, 9999, &id))
        return 0;

    int r = equipe_excluir(&camp, id);
    if (r != OK) return falhar(msg_erro(r));
    /* no banco, os jogadores saem pelo ON DELETE CASCADE */
    return concluir(banco_excluir_equipe(db, id), "Equipe excluída.");
}

static int cmd_cadastrar_jogador(const char *corpo) {
    char nick[TAM_NICK], nome[TAM_NOME];
    int id_equipe, id;
    if (!ler_txt(corpo, "nick", nick, TAM_NICK) || !ler_txt(corpo, "nome", nome, TAM_NOME) ||
        !ler_num(corpo, "equipe", 1, 9999, &id_equipe))
        return 0;

    int r = jogador_cadastrar(&camp, nick, nome, id_equipe, &id);
    if (r != OK) return falhar(msg_erro(r));

    char texto[64];
    snprintf(texto, sizeof(texto), "Jogador cadastrado com id %d.", id);
    return concluir(banco_inserir_jogador(db, jogador_buscar_nick(&camp, nick)), texto);
}

/* Como no main: o jogador é identificado pelo nick; o model usa o id. */
static int cmd_alterar_jogador(const char *corpo) {
    char nick[TAM_NICK], nome[TAM_NOME];
    int id_equipe;
    if (!ler_txt(corpo, "nick", nick, TAM_NICK) || !ler_txt(corpo, "nome", nome, TAM_NOME) ||
        !ler_num(corpo, "equipe", 1, 9999, &id_equipe))
        return 0;

    Jogador *j = jogador_buscar_nick(&camp, nick);
    if (j == NULL) return falhar(msg_erro(ERR_NAO_ENCONTRADO));

    int r = jogador_alterar(&camp, j->id, nome, id_equipe);
    if (r != OK) return falhar(msg_erro(r));
    return concluir(banco_atualizar_jogador(db, j), "Jogador alterado.");
}

static int cmd_excluir_jogador(const char *corpo) {
    char nick[TAM_NICK];
    if (!ler_txt(corpo, "nick", nick, TAM_NICK))
        return 0;

    Jogador *j = jogador_buscar_nick(&camp, nick);
    if (j == NULL) return falhar(msg_erro(ERR_NAO_ENCONTRADO));

    int id = j->id;   /* guarda antes: o excluir desloca o array */
    int r = jogador_excluir(&camp, id);
    if (r != OK) return falhar(msg_erro(r));
    return concluir(banco_excluir_jogador(db, id), "Jogador excluído.");
}

static int cmd_gerar_rodada(void) {
    int qtd = 0;
    int r = gerar_proxima_rodada(&camp, &qtd);
    if (r == ERR_INVALIDO) return falhar("São necessárias 8 equipes para gerar a rodada 1.");
    if (r != OK) return falhar(msg_erro(r));

    /* grava só as partidas novas (as últimas qtd do array) */
    int r_banco = OK;
    for (int i = camp.n_partidas - qtd; i < camp.n_partidas; i++)
        if (banco_inserir_partida(db, &camp.partidas[i]) != OK)
            r_banco = ERR_BANCO;

    char texto[64];
    snprintf(texto, sizeof(texto), "%d partida(s) gerada(s).", qtd);
    return concluir(r_banco, texto);
}

static int cmd_resultado(const char *corpo) {
    int id, a, b;
    if (!ler_num(corpo, "id", 1, 9999, &id) || !ler_num(corpo, "a", 0, 999, &a) ||
        !ler_num(corpo, "b", 0, 999, &b))
        return 0;

    int r = partida_registrar_resultado(&camp, id, a, b);
    if (r == ERR_INVALIDO) return falhar("Placar inválido: não existe empate.");
    if (r == ERR_BLOQUEADO) return falhar("Só dá para alterar partidas da rodada atual.");
    if (r != OK) return falhar(msg_erro(r));

    int pos = busca_linear_partida(camp.partidas, camp.n_partidas, id);
    return concluir(banco_atualizar_partida(db, &camp.partidas[pos]), "Resultado registrado.");
}

/* ============================================================
   BUSCAS (GET)
   ============================================================ */

/* Equipe pelo código (busca binária) + jogadores dela (busca linear). */
static void api_buscar_equipe(const char *query) {
    int id;
    if (!ler_num(query, "id", 1, 9999, &id)) {
        escrever("{\"ok\":false,\"msg\":");
        escrever_str(msg);
        escrever("}");
        return;
    }

    Equipe *e = equipe_buscar(&camp, id);
    if (e == NULL) {
        escrever("{\"ok\":false,\"msg\":");
        escrever_str(msg_erro(ERR_NAO_ENCONTRADO));
        escrever("}");
        return;
    }

    int indices[MAX_JOG];
    int qtd = busca_linear_jogadores_equipe(camp.jogadores, camp.n_jogadores, id, indices);

    escrever("{\"ok\":true,\"equipe\":");
    json_equipe(&camp, e);
    escrever(",\"jogadores\":[");
    for (int k = 0; k < qtd; k++) {
        if (k > 0) escrever(",");
        json_jogador(&camp.jogadores[indices[k]]);
    }
    escrever("]}");
}

/* Jogador pelo nick (busca linear, sem diferenciar maiúsculas). */
static void api_buscar_jogador(const char *query) {
    char nick[TAM_NICK];
    if (!ler_txt(query, "nick", nick, TAM_NICK)) {
        escrever("{\"ok\":false,\"msg\":");
        escrever_str(msg);
        escrever("}");
        return;
    }

    Jogador *j = jogador_buscar_nick(&camp, nick);
    if (j == NULL) {
        escrever("{\"ok\":false,\"msg\":");
        escrever_str(msg_erro(ERR_NAO_ENCONTRADO));
        escrever("}");
        return;
    }
    escrever("{\"ok\":true,\"jogador\":");
    json_jogador(j);
    escrever("}");
}

/* ============================================================
   HTTP
   Um pedido por vez: lê, responde e fecha a conexão.
   ============================================================ */

static void enviar_tudo(sock_t s, const char *dados, size_t tam) {
    while (tam > 0) {
        int k = send(s, dados, (int)tam, 0);
        if (k <= 0) return;
        dados += k;
        tam -= (size_t)k;
    }
}

static void responder(sock_t s, const char *status, const char *tipo, const char *dados, size_t tam) {
    char cab[256];
    int k = snprintf(cab, sizeof(cab),
                     "HTTP/1.1 %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %lu\r\n"
                     "Cache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n",
                     status, tipo, (unsigned long)tam);
    enviar_tudo(s, cab, (size_t)k);
    enviar_tudo(s, dados, tam);
}

static void responder_json(sock_t s) {
    responder(s, "200 OK", "application/json; charset=utf-8", resp, resp_n);
}

static void responder_texto(sock_t s, const char *status, const char *texto) {
    responder(s, status, "text/plain; charset=utf-8", texto, strlen(texto));
}

/* Manda um arquivo da pasta web/. Só os três da lista: nada de "../". */
static void responder_arquivo(sock_t s, const char *caminho) {
    const char *arquivo, *tipo;
    if (strcmp(caminho, "/") == 0 || strcmp(caminho, "/index.html") == 0) {
        arquivo = "web/index.html"; tipo = "text/html; charset=utf-8";
    } else if (strcmp(caminho, "/style.css") == 0) {
        arquivo = "web/style.css";  tipo = "text/css; charset=utf-8";
    } else if (strcmp(caminho, "/app.js") == 0) {
        arquivo = "web/app.js";     tipo = "application/javascript; charset=utf-8";
    } else {
        responder_texto(s, "404 Not Found", "Página não encontrada.");
        return;
    }

    FILE *f = fopen(arquivo, "rb");
    if (f == NULL) {
        responder_texto(s, "404 Not Found", "Arquivo não encontrado. Rode o servidor na pasta do projeto.");
        return;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *dados = malloc((size_t)tam + 1);
    if (dados == NULL) {
        fclose(f);
        responder_texto(s, "500 Internal Server Error", "Sem memória.");
        return;
    }
    size_t lidos = fread(dados, 1, (size_t)tam, f);
    fclose(f);
    responder(s, "200 OK", tipo, dados, lidos);
    free(dados);
}

/* Compara o começo de a com b sem diferenciar maiúsculas. */
static int comeca_com(const char *a, const char *b) {
    while (*b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return 1;
}

/* Procura um cabeçalho (ex.: "content-length:") e devolve o valor, ou NULL. */
static const char *cabecalho(const char *req, const char *fim_cab, const char *nome) {
    const char *linha = strstr(req, "\r\n");
    while (linha != NULL && linha < fim_cab) {
        linha += 2;
        if (comeca_com(linha, nome)) {
            const char *v = linha + strlen(nome);
            while (*v == ' ') v++;
            return v;
        }
        linha = strstr(linha, "\r\n");
    }
    return NULL;
}

static void atender(sock_t s) {
    static char req[TAM_REQ];
    int n = 0;
    char *fim_cab = NULL;

    /* 1. lê até o fim do cabeçalho (linha em branco) */
    while (n < TAM_REQ - 1) {
        int k = recv(s, req + n, TAM_REQ - 1 - n, 0);
        if (k <= 0) return;   /* fechou ou demorou demais */
        n += k;
        req[n] = '\0';
        fim_cab = strstr(req, "\r\n\r\n");
        if (fim_cab != NULL) break;
    }
    if (fim_cab == NULL) {
        responder_texto(s, "413 Payload Too Large", "Pedido grande demais.");
        return;
    }

    /* 2. lê o corpo, se tiver (Content-Length diz o tamanho) */
    int ini_corpo = (int)(fim_cab - req) + 4;
    const char *cl = cabecalho(req, fim_cab, "content-length:");
    long tam_corpo = (cl != NULL) ? strtol(cl, NULL, 10) : 0;
    if (tam_corpo < 0 || ini_corpo + tam_corpo > TAM_REQ - 1) {
        responder_texto(s, "413 Payload Too Large", "Pedido grande demais.");
        return;
    }
    while (n < ini_corpo + tam_corpo) {
        int k = recv(s, req + n, TAM_REQ - 1 - n, 0);
        if (k <= 0) return;
        n += k;
    }
    req[ini_corpo + tam_corpo] = '\0';
    const char *corpo = req + ini_corpo;

    /* 3. primeira linha: "GET /api/estado HTTP/1.1" */
    char metodo[8], caminho[512];
    if (sscanf(req, "%7s %511s", metodo, caminho) != 2) {
        responder_texto(s, "400 Bad Request", "Pedido inválido.");
        return;
    }
    char *query = strchr(caminho, '?');
    if (query != NULL) *query++ = '\0';
    else query = "";

    resp_n = 0;
    resp[0] = '\0';

    /* 4. rotas */
    if (strcmp(metodo, "GET") == 0) {
        if (strcmp(caminho, "/api/estado") == 0) {
            escrever("{\"ok\":true,\"estado\":");
            json_estado(&camp);
            escrever("}");
            responder_json(s);
        } else if (strcmp(caminho, "/api/equipe") == 0) {
            api_buscar_equipe(query);
            responder_json(s);
        } else if (strcmp(caminho, "/api/jogador") == 0) {
            api_buscar_jogador(query);
            responder_json(s);
        } else {
            responder_arquivo(s, caminho);
        }
        return;
    }

    if (strcmp(metodo, "POST") != 0) {
        responder_texto(s, "405 Method Not Allowed", "Método não permitido.");
        return;
    }

    /* Só a nossa página manda esse cabeçalho. Um site de fora não consegue
       (o navegador bloqueia), então ninguém altera o campeonato por outra aba. */
    if (cabecalho(req, fim_cab, "x-esports:") == NULL) {
        responder_texto(s, "403 Forbidden", "Pedido recusado.");
        return;
    }

    int ok;
    if      (strcmp(caminho, "/api/equipes/cadastrar") == 0)   ok = cmd_cadastrar_equipe(corpo);
    else if (strcmp(caminho, "/api/equipes/alterar") == 0)     ok = cmd_alterar_equipe(corpo);
    else if (strcmp(caminho, "/api/equipes/excluir") == 0)     ok = cmd_excluir_equipe(corpo);
    else if (strcmp(caminho, "/api/jogadores/cadastrar") == 0) ok = cmd_cadastrar_jogador(corpo);
    else if (strcmp(caminho, "/api/jogadores/alterar") == 0)   ok = cmd_alterar_jogador(corpo);
    else if (strcmp(caminho, "/api/jogadores/excluir") == 0)   ok = cmd_excluir_jogador(corpo);
    else if (strcmp(caminho, "/api/rodada/gerar") == 0)        ok = cmd_gerar_rodada();
    else if (strcmp(caminho, "/api/partidas/resultado") == 0)  ok = cmd_resultado(corpo);
    else {
        responder_texto(s, "404 Not Found", "Comando não encontrado.");
        return;
    }

    /* toda resposta de comando já leva o estado novo: a tela redesenha direto */
    escrever("{\"ok\":%s,\"msg\":", ok ? "true" : "false");
    escrever_str(msg);
    escrever(",\"estado\":");
    json_estado(&camp);
    escrever("}");
    responder_json(s);
}

/* ============================================================
   MAIN
   Uso: esports_web [porta] [arquivo.db]
   ============================================================ */

int main(int argc, char *argv[]) {
    int porta = (argc > 1) ? atoi(argv[1]) : PORTA_PADRAO;
    const char *arquivo = (argc > 2) ? argv[2] : "esports.db";
    if (porta <= 0 || porta > 65535) porta = PORTA_PADRAO;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Não foi possível iniciar os sockets.\n");
        return 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);   /* navegador fechou a conexão: não derruba o programa */
#endif

    /* mesma carga do main.c */
    campeonato_init(&camp);
    if (banco_abrir(&db, arquivo) != OK) {
        printf("Não foi possível abrir o banco.\n");
        return 1;
    }
    banco_carregar(db, &camp);
    recalcular_classificacao(&camp);

    FILE *teste = fopen("web/index.html", "rb");
    if (teste == NULL)
        printf("Aviso: web/index.html não encontrado. Rode o servidor na pasta do projeto.\n");
    else
        fclose(teste);

    sock_t servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor == INVALID_SOCKET) {
        printf("Não foi possível criar o socket.\n");
        return 1;
    }
#ifndef _WIN32
    int sim = 1;   /* permite reabrir na mesma porta logo depois de fechar */
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &sim, sizeof(sim));
#endif

    /* 127.0.0.1: só este computador acessa */
    struct sockaddr_in end;
    memset(&end, 0, sizeof(end));
    end.sin_family = AF_INET;
    end.sin_port = htons((unsigned short)porta);
    end.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(servidor, (struct sockaddr *)&end, sizeof(end)) != 0 || listen(servidor, 8) != 0) {
        printf("Não foi possível usar a porta %d (já está em uso?).\n", porta);
        fechar_socket(servidor);
        banco_fechar(db);
        return 1;
    }

    printf("Campeonato carregado: %d equipe(s), %d jogador(es), %d partida(s).\n",
           camp.n_equipes, camp.n_jogadores, camp.n_partidas);
    printf("Abra no navegador: http://localhost:%d\n", porta);
    printf("Ctrl+C para encerrar.\n");
    fflush(stdout);

    while (1) {
        sock_t cliente = accept(servidor, NULL, NULL);
        if (cliente == INVALID_SOCKET) continue;

        /* Se o navegador abrir uma conexão e não mandar nada, desiste em 2 s
           (o servidor atende um pedido por vez e não pode ficar travado). */
#ifdef _WIN32
        DWORD espera = 2000;
        setsockopt(cliente, SOL_SOCKET, SO_RCVTIMEO, (const char *)&espera, sizeof(espera));
#else
        struct timeval espera = {2, 0};
        setsockopt(cliente, SOL_SOCKET, SO_RCVTIMEO, &espera, sizeof(espera));
#endif

        atender(cliente);
        shutdown(cliente, 1);   /* 1 = terminou de enviar (SD_SEND / SHUT_WR) */
        fechar_socket(cliente);
    }
}
