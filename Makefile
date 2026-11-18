# Campeonato de e-Sports. Um comando: make (no Windows: mingw32-make)
# Gera dois programas: esports (console) e esports_web (frontend no navegador)
CC     = gcc
CFLAGS = -Wall -Wextra -std=c99

SRC = main.c \
      models/campeonato.c models/equipe.c models/jogador.c models/partida.c \
      algoritmos/busca.c algoritmos/ordenacao.c \
      persistencia/banco.c

HDR = models/campeonato.h models/equipe.h models/jogador.h models/partida.h \
      algoritmos/busca.h algoritmos/ordenacao.h \
      persistencia/banco.h persistencia/sqlite3.h

SQLITE_O = persistencia/sqlite3.o

# O servidor web usa tudo menos o main.c (ele tem o próprio main)
SRC_WEB = web/servidor.c \
          models/campeonato.c models/equipe.c models/jogador.c models/partida.c \
          algoritmos/busca.c algoritmos/ordenacao.c \
          persistencia/banco.c

# Limpeza funciona no cmd do Windows e no Linux
ifeq ($(OS),Windows_NT)
    APAGAR = del /Q /F
    ARQS   = esports.exe esports_web.exe persistencia\sqlite3.o
    SOCKET = -lws2_32
else
    APAGAR = rm -f
    ARQS   = esports esports_web $(SQLITE_O)
    SOCKET =
endif

all: esports esports_web

esports: $(SRC) $(HDR) $(SQLITE_O)
	$(CC) $(CFLAGS) -o esports $(SRC) $(SQLITE_O)

# No Windows os sockets ficam na biblioteca ws2_32
esports_web: $(SRC_WEB) $(HDR) $(SQLITE_O)
	$(CC) $(CFLAGS) -o esports_web $(SRC_WEB) $(SQLITE_O) $(SOCKET)

# O SQLite compila uma vez só e sem -Wall: os avisos dele não são do projeto
$(SQLITE_O): persistencia/sqlite3.c persistencia/sqlite3.h
	$(CC) -c persistencia/sqlite3.c -o $(SQLITE_O)

clean:
	-$(APAGAR) $(ARQS)

.PHONY: all clean
