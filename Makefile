# Campeonato de e-Sports. Um comando: make (no Windows: mingw32-make)
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

# Limpeza funciona no cmd do Windows e no Linux
ifeq ($(OS),Windows_NT)
    APAGAR = del /Q /F
    ARQS   = esports.exe persistencia\sqlite3.o
else
    APAGAR = rm -f
    ARQS   = esports $(SQLITE_O)
endif

esports: $(SRC) $(HDR) $(SQLITE_O)
	$(CC) $(CFLAGS) -o esports $(SRC) $(SQLITE_O)

# O SQLite compila uma vez só e sem -Wall: os avisos dele não são do projeto
$(SQLITE_O): persistencia/sqlite3.c persistencia/sqlite3.h
	$(CC) -c persistencia/sqlite3.c -o $(SQLITE_O)

clean:
	-$(APAGAR) $(ARQS)

.PHONY: clean
