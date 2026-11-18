# Campeonato de e-Sports

Projeto Integrador: sistema de console em C para organizar um campeonato de e-Sports com 8 equipes, com persistência em SQLite.

## 1. Integrantes

| Nome | RA | Responsabilidade |
| --- | --- | --- |
| Gabriel Gurian | 10769963 | Núcleo do campeonato: regras, pareamentos, ranking, matriz, ordenação, leitura de entrada |
| Pedro Vidigal | 10436802 | Cadastros (equipes e jogadores), buscas, persistência SQLite, Makefile |

## 2. Problema e funcionalidades

**Problema:** organizar um campeonato com 8 equipes de até 5 jogadores, em 3 fases:
- **Suíço** de 3 rodadas, sem revanche;
- **semifinal** entre os 4 melhores do ranking (1º x 4º e 2º x 3º);
- **final** entre os vencedores das semifinais.

Os dados precisam sobreviver ao fechamento do programa.

**Funcionalidades:**
- **Equipes:** cadastrar (código digitado, único e > 0), alterar o nome, excluir (os jogadores dela vão junto), listar em ordem de código e buscar por código.
- **Jogadores:** cadastrar (nick único, sem diferenciar maiúsculas; até 5 por equipe), alterar nome e equipe, excluir, listar em ordem alfabética de nick, listar por equipe e buscar por nick.
- **Campeonato:** gerar a próxima rodada, listar as partidas da rodada atual, registrar ou alterar um resultado (só na rodada atual), ver o ranking, a matriz de confrontos e o campeão.
- **Regras validadas:**
  - código ou nick repetido; registro inexistente;
  - empate ou placar negativo;
  - equipe cheia;
  - cadastrar ou excluir equipe depois que o campeonato começou;
  - gerar rodada com partidas pendentes;
  - texto no lugar de número.
- **Persistência:** tudo é gravado no momento da operação. Ao reabrir, o programa recarrega os dados e deduz a fase atual pela última partida.

**Pontuação:** vitória vale 3 pontos e derrota vale 0; não existe empate. O saldo é a diferença de rounds. O desempate usa pontos, depois saldo, depois o menor código. Pontos, saldo, matriz e fase **não são salvos**: são recalculados a partir das partidas.

## 3. Algoritmos

| Algoritmo | Onde | Para quê |
| --- | --- | --- |
| **Busca binária** | `algoritmos/busca.c` → `busca_binaria_equipe` | Achar uma equipe pelo código. Funciona porque `equipes[]` fica **sempre ordenado por id** |
| **Inserção ordenada** | `models/equipe.c` → `equipe_inserir_ordenado` | Inserir a equipe na posição certa, deslocando as maiores para a direita (mesma ideia do insertion sort). É usada no cadastro e no carregamento do banco |
| **Busca linear** | `algoritmos/busca.c` | Jogador por nick (sem diferenciar maiúsculas, com uma comparação própria usando `tolower`), jogadores de uma equipe, jogador por id e partida por id |
| **Insertion sort do ranking** | `algoritmos/ordenacao.c` → `insertion_sort_ranking` | Ordena por pontos, depois saldo, depois menor id |
| **Insertion sort por nick** | `algoritmos/ordenacao.c` → `insertion_sort_jogadores_nick` | Listagem alfabética dos jogadores |
| **Matriz de confrontos** | `models/partida.c` → `confronto[8][8]` | Evita revanche no pareamento do suíço e é exibida como tabela |
| **Pareamento suíço** | `models/partida.c` → `parear_suico` | Ordena pelo ranking e, de cima para baixo, junta cada equipe livre com a próxima livre que ela ainda não enfrentou |

As duas ordenações trabalham sobre **arrays de ponteiros** (`Equipe *r[]`, `Jogador *v[]`). Os arrays originais não mudam de ordem, então a busca binária continua válida. Nenhuma busca ou ordenação usa `qsort`, `bsearch`, `WHERE` ou `ORDER BY`.

## 4. Banco de dados e driver

- **SGBD:** SQLite 3.54.1.
- **Driver:** a API C oficial do SQLite (`sqlite3.h`), usada pelo **amalgamation**: os arquivos `sqlite3.c` e `sqlite3.h` ficam dentro de `persistencia/` e são compilados junto com o projeto.
- **Arquivo do banco:** `esports.db`, criado na primeira execução.
- **Acesso:**
  - todo comando com valores usa `sqlite3_prepare_v2` + `sqlite3_bind_*` + `sqlite3_step` + `sqlite3_finalize`;
  - nenhum SQL é montado com `sprintf`, então um nome com apóstrofo (`O'Neil`) não quebra a query;
  - a criação das tabelas usa `sqlite3_exec`, porque são 3 comandos de uma vez;
  - `PRAGMA foreign_keys = ON` é executado logo depois de abrir a conexão, para o `ON DELETE CASCADE` funcionar.
- **Separação:** só `persistencia/` conhece o SQLite. Os models não incluem `sqlite3.h`, e o `main.c` chama primeiro o model (validação) e, se deu certo, a função do banco.

## 5. Justificativa

- **Não precisa de servidor:** o banco é um arquivo, e o SQLite compila com o próprio gcc em qualquer máquina. O professor compila e roda sem configurar nada.
- **Tem o que o projeto precisa:** chave primária, `UNIQUE` no nick, chave estrangeira com `ON DELETE CASCADE` e `CHECK` na fase.
- **API C direta:** sem camada intermediária, o que deixa claro onde está cada `INSERT`, `UPDATE`, `DELETE` e `SELECT`.

## 6. Tabelas

```sql
CREATE TABLE IF NOT EXISTS equipes (
    id   INTEGER PRIMARY KEY,
    nome TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS jogadores (
    id        INTEGER PRIMARY KEY,
    nick      TEXT    NOT NULL UNIQUE,
    nome      TEXT    NOT NULL,
    id_equipe INTEGER NOT NULL REFERENCES equipes(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS partidas (
    id         INTEGER PRIMARY KEY,
    fase       INTEGER NOT NULL CHECK (fase BETWEEN 1 AND 3),
    rodada     INTEGER NOT NULL,
    id_a       INTEGER NOT NULL REFERENCES equipes(id),
    id_b       INTEGER NOT NULL REFERENCES equipes(id),
    placar_a   INTEGER NOT NULL DEFAULT 0,
    placar_b   INTEGER NOT NULL DEFAULT 0,
    finalizada INTEGER NOT NULL DEFAULT 0
);
```

- **`equipes.id`:** é o código digitado pelo usuário, e não um autoincremento.
- **`jogadores.id_equipe`:** aponta para a equipe. `ON DELETE CASCADE` faz o banco apagar os jogadores quando a equipe é excluída. Na memória, `equipe_excluir` faz o mesmo nos arrays.
- **`partidas.id_a` e `id_b`:** apontam para as equipes, sem cascade. A exclusão de equipe só é permitida antes da 1ª partida.
- **`partidas.fase`:** 1 = suíço, 2 = semifinal, 3 = final. `rodada` vai de 1 a 3 no suíço e é 0 no mata-mata. `finalizada` vale 1 quando o resultado já foi registrado.
- **O que não vai para o banco:** pontos, vitórias, derrotas, saldo, matriz e fase. Ao abrir, `recalcular_classificacao` reconstrói tudo a partir das partidas finalizadas.

O mesmo SQL está em `persistencia/schema.sql` e embutido em `persistencia/banco.c`.

## 7. Configuração, compilação e execução

**Requisito:** gcc (MinGW-w64 no Windows). Não é preciso instalar o SQLite: ele vem no projeto.

**Com o Makefile** (no Windows, o `make` do MinGW se chama `mingw32-make`):

```
mingw32-make
```

A primeira compilação gera `persistencia/sqlite3.o`, o que leva uns 15 s; as seguintes reaproveitam esse arquivo. Para apagar o que foi gerado, rode `mingw32-make clean`.

**Sem make**, com o comando gcc completo:

```
gcc -c persistencia/sqlite3.c -o persistencia/sqlite3.o
gcc -Wall -Wextra -std=c99 -o esports main.c models/campeonato.c models/equipe.c models/jogador.c models/partida.c algoritmos/busca.c algoritmos/ordenacao.c persistencia/banco.c persistencia/sqlite3.o
```

**Execução:**

```
chcp 65001
esports
```

- O `chcp 65001` faz o console do Windows mostrar os acentos corretamente.
- O programa usa o `esports.db` da pasta atual. O entregue já tem **8 equipes e 27 jogadores** cadastrados para a demonstração. Para começar do zero, apague o `esports.db`.

### Frontend (navegador)

Além do console, o projeto tem uma interface web. O `mingw32-make` gera os dois programas: `esports` e `esports_web`.

```
esports_web
```

Depois é só abrir **http://localhost:8080** no navegador. Para usar outra porta ou outro banco: `esports_web 8081 outro.db`. Para parar, use Ctrl+C.

Sem make:

```
gcc -Wall -Wextra -std=c99 -o esports_web web/servidor.c models/campeonato.c models/equipe.c models/jogador.c models/partida.c algoritmos/busca.c algoritmos/ordenacao.c persistencia/banco.c persistencia/sqlite3.o -lws2_32
```

(no Linux, sem o `-lws2_32`)

**Como funciona:**
- `web/servidor.c` é um servidor HTTP pequeno escrito em C, com sockets (Winsock no Windows). Ele reaproveita **os mesmos** models, algoritmos e banco do console. Só o `main.c` fica de fora, porque o servidor tem o próprio `main`.
- O navegador não tem regra nenhuma. Ele pede `GET /api/estado` (um JSON com equipes, ranking, jogadores, partidas, matriz e campeão), desenha a tela e envia os comandos por `POST /api/...`.
- Cada comando segue o mesmo padrão do `main.c`: lê e valida os campos (como `ler_int`/`ler_texto`), chama o model e, se deu OK, grava no banco.
- O ranking e a lista por nick já chegam ordenados pelo insertion sort do C. A busca de equipe usa a busca binária, e a de jogador usa a busca linear.
- O servidor só aceita conexões do próprio computador (127.0.0.1). Os comandos exigem o cabeçalho `X-Esports`, então outro site aberto no navegador não consegue alterar o campeonato.
- `web/index.html`, `web/style.css` e `web/app.js` são lidos do disco a cada pedido. Por isso o servidor precisa ser executado na pasta do projeto.

**Atenção:** não use o console e o servidor ao mesmo tempo no mesmo banco. Cada programa carrega os dados na memória ao abrir e não vê o que o outro gravou depois disso.

**Estrutura:**

```
main.c            menus e leitura validada (ler_int, ler_texto)
models/           campeonato (structs, fase), equipe, jogador, partida  (sem SQLite)
algoritmos/       busca (binária e lineares), ordenacao (insertion sort) (sem printf)
persistencia/     banco.c/.h (SQLite), schema.sql, sqlite3.c/.h
web/              servidor.c (HTTP + JSON em C), index.html, style.css, app.js
Makefile
```

## 8. Referências

- SQLite. *An Introduction To The SQLite C/C++ Interface*. https://www.sqlite.org/cintro.html
- SQLite. *C/C++ Interface For SQLite Version 3* (prepare, bind, step, finalize, exec). https://www.sqlite.org/c3ref/intro.html
- SQLite. *SQLite Foreign Key Support* (`PRAGMA foreign_keys`, `ON DELETE CASCADE`). https://www.sqlite.org/foreignkeys.html
- SQLite. *The SQLite Amalgamation*. https://www.sqlite.org/amalgamation.html
- cppreference.com. *C reference* (`fgets`, `strtol`, `strncpy`, `tolower`). https://en.cppreference.com/w/c
- CORMEN, T. H. et al. *Algoritmos: teoria e prática*. Busca binária e ordenação por inserção.
- WinLibs. *GCC + MinGW-w64 para Windows*. https://winlibs.com
