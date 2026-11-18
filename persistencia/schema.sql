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
