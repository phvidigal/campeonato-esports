'use strict';
/* ============================================================
   Frontend do campeonato.
   Aqui NÃO tem regra de negócio: a página pede os dados ao servidor em C
   (web/servidor.c), desenha na tela e manda os comandos. Quem valida
   (código repetido, equipe cheia, empate, fase...) continua sendo o C.
   O ranking e a lista por nick já chegam ordenados pelo insertion sort do C.
   ============================================================ */

const FASE = { CADASTRO: 0, SUICO: 1, SEMI: 2, FINAL: 3, ENCERRADO: 4 };

let estado = null;          /* último JSON recebido de /api/estado */
let equipeAberta = null;    /* código da equipe mostrada na janela */

const $ = (seletor) => document.querySelector(seletor);

/* Escapa o texto antes de pôr no HTML (um nome com "<" não vira tag). */
function esc(texto) {
  return String(texto).replace(/[&<>"']/g, (c) => (
    { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]
  ));
}

/* ---------------- comunicação com o servidor ---------------- */

async function api(metodo, caminho, dados) {
  const opcoes = { method: metodo, headers: {} };
  if (metodo === 'POST') {
    opcoes.headers['X-Esports'] = '1';   /* o servidor só aceita POST com este cabeçalho */
    opcoes.headers['Content-Type'] = 'application/x-www-form-urlencoded';
    opcoes.body = new URLSearchParams(dados || {}).toString();
  }

  let json;
  try {
    const resposta = await fetch(caminho, opcoes);
    json = await resposta.json();
  } catch (e) {
    avisar('Sem conexão com o servidor. Ele está rodando?', true);
    return null;
  }

  if (json.msg) avisar(json.msg, !json.ok);
  /* em caso de erro nada mudou: não redesenha, para não apagar o que foi digitado */
  if (json.ok && json.estado) {
    estado = json.estado;
    desenhar();
  }
  return json;
}

let timerAviso;
function avisar(texto, erro) {
  const el = $('#aviso');
  el.textContent = texto;
  el.className = 'aviso visivel' + (erro ? ' erro' : '');
  clearTimeout(timerAviso);
  timerAviso = setTimeout(() => { el.className = 'aviso' + (erro ? ' erro' : ''); }, 3500);
}

/* ---------------- ajudantes ---------------- */

function equipePorId(id) {
  return estado.equipes.find((e) => e.id === id);
}

function nomeEquipe(id) {
  const e = equipePorId(id);
  return e ? e.nome : '?';
}

function nomeRodada(fase, rodada) {
  if (fase === FASE.SUICO) return `Suíço · Rodada ${rodada}`;
  if (fase === FASE.SEMI) return 'Semifinal';
  if (fase === FASE.FINAL) return 'Final';
  return '';
}

function textoFase() {
  switch (estado.fase) {
    case FASE.CADASTRO: return `Cadastro · ${estado.equipes.length}/${estado.max_equipes} equipes`;
    case FASE.SUICO: return `Suíço · Rodada ${estado.rodada} de 3`;
    case FASE.SEMI: return 'Semifinal';
    case FASE.FINAL: return 'Final';
    default: return 'Encerrado';
  }
}

function vencedor(p) {
  return p.placar_a > p.placar_b ? p.a : p.b;
}

/* A rodada atual é o par (fase, rodada) da última partida, igual ao C. */
function partidasDaRodadaAtual() {
  const lista = estado.partidas;
  if (lista.length === 0) return [];
  const ultima = lista[lista.length - 1];
  return lista.filter((p) => p.fase === ultima.fase && p.rodada === ultima.rodada);
}

/* Agrupa as partidas por rodada, mantendo a ordem de criação. */
function rodadas() {
  const grupos = [];
  for (const p of estado.partidas) {
    const chave = `${p.fase}-${p.rodada}`;
    let g = grupos.find((x) => x.chave === chave);
    if (!g) {
      g = { chave, fase: p.fase, rodada: p.rodada, partidas: [] };
      grupos.push(g);
    }
    g.partidas.push(p);
  }
  return grupos;
}

/* ---------------- desenho ---------------- */

function desenhar() {
  desenharTopo();
  desenharPartidas();
  desenharClassificacao();
  desenharEquipes();
  desenharJogadores();
}

function desenharTopo() {
  $('#fase-texto').textContent = textoFase();

  /* Etapas: Cadastro, R1, R2, R3, Semi, Final, Campeão */
  const nomes = ['Cadastro', 'Rodada 1', 'Rodada 2', 'Rodada 3', 'Semifinal', 'Final', 'Campeão'];
  let atual;
  if (estado.fase === FASE.CADASTRO) atual = 0;
  else if (estado.fase === FASE.SUICO) atual = estado.rodada;
  else if (estado.fase === FASE.SEMI) atual = 4;
  else if (estado.fase === FASE.FINAL) atual = 5;
  else atual = 6;

  $('#etapas').innerHTML = nomes.map((n, i) => {
    const classe = i < atual ? 'feita' : (i === atual ? 'atual' : '');
    return `<li class="${classe}">${n}</li>`;
  }).join('');
}

function linhaPartidaEditavel(p) {
  let classeA = '', classeB = '';
  if (p.finalizada) {
    classeA = p.placar_a > p.placar_b ? 'venceu' : 'perdeu';
    classeB = p.placar_b > p.placar_a ? 'venceu' : 'perdeu';
  }
  return `
    <form class="partida ${p.finalizada ? 'finalizada' : ''}" data-acao="resultado">
      <input type="hidden" name="id" value="${p.id}">
      <span class="pid">#${p.id}</span>
      <div class="lado a ${classeA}">${esc(nomeEquipe(p.a))}<small>código ${p.a}</small></div>
      <input name="a" type="number" min="0" max="999" required aria-label="Placar de ${esc(nomeEquipe(p.a))}"
             value="${p.finalizada ? p.placar_a : ''}" placeholder="–">
      <span class="x">×</span>
      <input name="b" type="number" min="0" max="999" required aria-label="Placar de ${esc(nomeEquipe(p.b))}"
             value="${p.finalizada ? p.placar_b : ''}" placeholder="–">
      <div class="lado b ${classeB}">${esc(nomeEquipe(p.b))}<small>código ${p.b}</small></div>
      <button class="btn mini ${p.finalizada ? '' : 'primario'}">${p.finalizada ? 'Alterar' : 'Salvar'}</button>
    </form>`;
}

function linhaPartidaFixa(p) {
  const v = vencedor(p);
  return `
    <div class="placar-fixo">
      <span class="a ${v === p.a ? 'venceu' : ''}">${esc(nomeEquipe(p.a))}</span>
      <span class="num">${p.placar_a} × ${p.placar_b}</span>
      <span class="${v === p.b ? 'venceu' : ''}">${esc(nomeEquipe(p.b))}</span>
    </div>`;
}

function rotuloBotaoGerar() {
  if (estado.fase === FASE.CADASTRO) return 'Gerar rodada 1';
  if (estado.fase === FASE.SUICO) return estado.rodada < 3 ? `Gerar rodada ${estado.rodada + 1}` : 'Gerar semifinais';
  if (estado.fase === FASE.SEMI) return 'Gerar final';
  return '';
}

function desenharPartidas() {
  const atuais = partidasDaRodadaAtual();
  const encerrado = estado.fase === FASE.ENCERRADO;

  /* campeão */
  $('#campeao').innerHTML = encerrado ? `
    <div class="banner-campeao">
      <div class="trofeu" aria-hidden="true">🏆</div>
      <div class="rotulo">Campeão</div>
      <div class="nome">${esc(nomeEquipe(estado.campeao))}</div>
    </div>` : '';

  /* título e botão de gerar */
  $('#rodada-titulo').textContent = atuais.length
    ? `Rodada atual · ${nomeRodada(atuais[0].fase, atuais[0].rodada)}`
    : 'Rodada atual';

  const botao = $('#btn-gerar');
  const podeGerar = estado.fase <= FASE.SEMI;
  botao.hidden = !podeGerar;
  botao.textContent = rotuloBotaoGerar();

  let dica = '';
  if (estado.fase === FASE.CADASTRO && estado.equipes.length < estado.max_equipes)
    dica = `Faltam ${estado.max_equipes - estado.equipes.length} equipe(s) para começar.`;
  else if (estado.fase !== FASE.CADASTRO && !estado.rodada_concluida)
    dica = `${atuais.filter((p) => !p.finalizada).length} partida(s) sem resultado.`;
  else if (estado.fase === FASE.FINAL)
    dica = 'Registre o resultado da final.';
  $('#rodada-dica').textContent = dica;

  /* partidas da rodada atual (as únicas que o C deixa alterar) */
  const lista = $('#rodada-lista');
  if (atuais.length === 0) {
    lista.innerHTML = `<p class="vazio">Nenhuma partida gerada ainda. Cadastre ${estado.max_equipes} equipes e gere a rodada 1.</p>`;
  } else {
    lista.innerHTML = `<div class="lista-partidas">${atuais.map(linhaPartidaEditavel).join('')}</div>`;
  }

  /* chaveamento (semifinal e final) */
  const semis = estado.partidas.filter((p) => p.fase === FASE.SEMI);
  const final = estado.partidas.find((p) => p.fase === FASE.FINAL);
  $('#chave-card').hidden = semis.length === 0;
  if (semis.length) {
    const confronto = (p) => {
      const v = p.finalizada ? vencedor(p) : null;
      const time = (id, placar) => `
        <div class="time ${v === id ? 'venceu' : ''}">
          <span>${esc(nomeEquipe(id))}</span><span class="num">${p.finalizada ? placar : ''}</span>
        </div>`;
      return `<div class="confronto ${p.finalizada ? '' : 'aguardando'}">${time(p.a, p.placar_a)}${time(p.b, p.placar_b)}</div>`;
    };
    $('#chave').innerHTML = `
      <div class="coluna"><span class="titulo-coluna">Semifinais</span>${semis.map(confronto).join('')}</div>
      <div class="coluna"><span class="titulo-coluna">Final</span>${final ? confronto(final) : '<div class="taca">Aguardando semifinais</div>'}</div>
      <div class="coluna"><span class="titulo-coluna">Campeão</span>
        <div class="taca ${encerrado ? 'definida' : ''}">${encerrado ? '🏆 ' + esc(nomeEquipe(estado.campeao)) : 'A definir'}</div>
      </div>`;
  }

  /* rodadas anteriores (só leitura) */
  const anteriores = rodadas().slice(0, -1).reverse();
  $('#historico-card').hidden = anteriores.length === 0;
  $('#historico').innerHTML = anteriores.map((g) => `
    <div class="rodada-antiga">
      <h3>${nomeRodada(g.fase, g.rodada)}</h3>
      <div class="lista-partidas">${g.partidas.map(linhaPartidaFixa).join('')}</div>
    </div>`).join('');
}

function desenharClassificacao() {
  /* ranking: a ordem vem pronta do insertion_sort_ranking do C */
  const mostrarCorte = estado.fase >= FASE.SUICO;
  const linhas = estado.ranking.map((id, i) => {
    const e = equipePorId(id);
    const saldo = e.saldo > 0 ? `+${e.saldo}` : `${e.saldo}`;
    const classeSaldo = e.saldo > 0 ? 'positivo' : (e.saldo < 0 ? 'negativo' : '');
    const classifica = mostrarCorte && i < 4;
    return `
      <tr class="${classifica ? 'classifica' : ''}">
        <td class="pos">${i + 1}</td>
        <td>${esc(e.nome)} ${classifica ? '<span class="tag">semifinal</span>' : ''}</td>
        <td class="num">${e.id}</td>
        <td class="num pts">${e.pontos}</td>
        <td class="num">${e.vitorias}</td>
        <td class="num">${e.derrotas}</td>
        <td class="num ${classeSaldo}">${saldo}</td>
      </tr>`;
  }).join('');

  $('#ranking').innerHTML = estado.equipes.length === 0
    ? '<tbody><tr><td class="vazio">Nenhuma equipe cadastrada.</td></tr></tbody>'
    : `<thead><tr><th>#</th><th>Equipe</th><th class="num">Código</th><th class="num">Pts</th>
         <th class="num">V</th><th class="num">D</th><th class="num">Saldo</th></tr></thead>
       <tbody>${linhas}</tbody>`;

  /* matriz: mesma ordem do array equipes[] */
  const eq = estado.equipes;
  if (eq.length === 0) {
    $('#matriz').innerHTML = '<tbody><tr><td class="vazio">Nenhuma equipe cadastrada.</td></tr></tbody>';
    return;
  }
  const cabeca = eq.map((e) => `<th title="${esc(e.nome)}">${e.id}</th>`).join('');
  const corpo = eq.map((e, i) => {
    const celulas = eq.map((_, j) => {
      if (i === j) return '<td class="eu">×</td>';
      const v = estado.matriz[i][j];
      if (v === 1) return '<td class="v">V</td>';
      if (v === -1) return '<td class="d">D</td>';
      return '<td class="nada">–</td>';
    }).join('');
    return `<tr><th class="linha">${e.id} · ${esc(e.nome)}</th>${celulas}</tr>`;
  }).join('');
  $('#matriz').innerHTML = `<thead><tr><th></th>${cabeca}</tr></thead><tbody>${corpo}</tbody>`;
}

function desenharEquipes() {
  const cadastroAberto = estado.fase === FASE.CADASTRO;
  const form = document.querySelector('form[data-acao="cadastrar-equipe"]');
  $('#equipe-bloqueio').hidden = cadastroAberto;
  form.querySelectorAll('input, button').forEach((el) => { el.disabled = !cadastroAberto; });

  $('#equipes-contagem').textContent = `${estado.equipes.length}/${estado.max_equipes}`;

  const max = estado.jogadores_por_equipe;
  $('#equipes-grade').innerHTML = estado.equipes.length === 0
    ? '<p class="vazio">Nenhuma equipe cadastrada.</p>'
    : estado.equipes.map((e) => `
      <button class="card-equipe" type="button" data-equipe="${e.id}">
        <span class="codigo">Código ${e.id}</span>
        <strong>${esc(e.nome)}</strong>
        <span class="linha"><span>${e.jogadores}/${max} jogadores</span><span>${e.pontos} pts</span></span>
        <span class="barra ${e.jogadores >= max ? 'cheia' : ''}"><span style="width:${(e.jogadores / max) * 100}%"></span></span>
      </button>`).join('');

  /* os selects de equipe (cadastro de jogador, edição e filtro) */
  const opcoes = estado.equipes.map((e) =>
    `<option value="${e.id}">${e.id} · ${esc(e.nome)} (${e.jogadores}/${max})</option>`).join('');
  document.querySelectorAll('.sel-equipes').forEach((sel) => {
    const valor = sel.value;
    sel.innerHTML = opcoes;
    if (valor) sel.value = valor;
  });

  const filtro = $('#filtro-equipe');
  const valorFiltro = filtro.value;
  filtro.innerHTML = '<option value="">Todos (por nick)</option>' +
    estado.equipes.map((e) => `<option value="${e.id}">${esc(e.nome)}</option>`).join('');
  filtro.value = equipePorId(Number(valorFiltro)) ? valorFiltro : '';

  /* se a janela de equipe estiver aberta, atualiza (ou fecha se ela sumiu) */
  if (equipeAberta !== null && $('#dlg-equipe').open) {
    if (equipePorId(equipeAberta)) abrirEquipe(equipeAberta, true);
    else $('#dlg-equipe').close();
  }
}

function desenharJogadores() {
  const filtro = Number($('#filtro-equipe').value) || 0;
  /* estado.jogadores já vem em ordem de nick (insertion sort no C) */
  const lista = filtro ? estado.jogadores.filter((j) => j.equipe === filtro) : estado.jogadores;

  if (lista.length === 0) {
    $('#jogadores').innerHTML = `<tbody><tr><td class="vazio">${filtro ? 'Nenhum jogador nesta equipe.' : 'Nenhum jogador cadastrado.'}</td></tr></tbody>`;
    return;
  }

  $('#jogadores').innerHTML = `
    <thead><tr><th class="num">Id</th><th>Nick</th><th>Nome</th><th>Equipe</th><th></th></tr></thead>
    <tbody>${lista.map((j) => `
      <tr>
        <td class="num">${j.id}</td>
        <td class="nick">${esc(j.nick)}</td>
        <td>${esc(j.nome)}</td>
        <td>${j.equipe} · ${esc(nomeEquipe(j.equipe))}</td>
        <td class="acoes-linha">
          <button class="btn mini" type="button" data-editar="${esc(j.nick)}">Editar</button>
          <button class="btn mini perigo" type="button" data-excluir="${esc(j.nick)}">Excluir</button>
        </td>
      </tr>`).join('')}
    </tbody>`;
}

/* ---------------- janelas ---------------- */

/* Usa /api/equipe: busca binária da equipe e busca linear dos jogadores no C. */
async function abrirEquipe(id, silencioso) {
  const r = silencioso
    ? await fetch('/api/equipe?id=' + encodeURIComponent(id)).then((x) => x.json()).catch(() => null)
    : await api('GET', '/api/equipe?id=' + encodeURIComponent(id));
  if (!r || !r.ok) return;

  const e = r.equipe;
  equipeAberta = e.id;
  const dlg = $('#dlg-equipe');
  $('#dlg-equipe-titulo').textContent = `${e.id} · ${e.nome}`;
  $('#dlg-equipe-stats').innerHTML = `
    <div><b>${e.pontos}</b><span>pontos</span></div>
    <div><b>${e.vitorias}</b><span>vitórias</span></div>
    <div><b>${e.derrotas}</b><span>derrotas</span></div>
    <div><b>${e.saldo > 0 ? '+' : ''}${e.saldo}</b><span>saldo</span></div>`;
  $('#dlg-equipe-jogadores').innerHTML = r.jogadores.length
    ? r.jogadores.map((j) => `<li><b>${esc(j.nick)}</b><span>${esc(j.nome)}</span></li>`).join('')
    : '<li><span>Nenhum jogador nesta equipe.</span></li>';

  const form = dlg.querySelector('form');
  form.elements.id.value = e.id;
  if (!silencioso) form.elements.nome.value = e.nome;

  const excluir = $('#btn-excluir-equipe');
  excluir.disabled = estado.fase !== FASE.CADASTRO;
  excluir.title = excluir.disabled ? 'Só dá para excluir antes da 1ª partida.' : '';

  if (!dlg.open) dlg.showModal();
}

function abrirEdicaoJogador(nick) {
  const j = estado.jogadores.find((x) => x.nick === nick);
  if (!j) return;
  const form = $('#dlg-jogador form');
  form.elements.nick.value = j.nick;
  form.elements.nome.value = j.nome;
  form.elements.equipe.value = j.equipe;
  $('#dlg-jogador').showModal();
}

/* ---------------- ações dos formulários ---------------- */

const ACOES = {
  'cadastrar-equipe': (d) => api('POST', '/api/equipes/cadastrar', d),
  'alterar-equipe': (d) => api('POST', '/api/equipes/alterar', d),
  'cadastrar-jogador': (d) => api('POST', '/api/jogadores/cadastrar', d),
  'alterar-jogador': (d) => api('POST', '/api/jogadores/alterar', d),
  'resultado': (d) => api('POST', '/api/partidas/resultado', d),

  'buscar-equipe': async (d) => {
    await abrirEquipe(d.id);
    return null;   /* não limpa o formulário */
  },

  'buscar-jogador': async (d) => {
    const r = await api('GET', '/api/jogador?nick=' + encodeURIComponent(d.nick));
    const caixa = $('#busca-jogador');
    if (!r || !r.ok) { caixa.hidden = true; return null; }
    const j = r.jogador;
    caixa.innerHTML = `Jogador <strong>#${j.id}</strong>: <span class="nick">${esc(j.nick)}</span>
      (${esc(j.nome)}) · equipe ${j.equipe} ${esc(nomeEquipe(j.equipe))}`;
    caixa.hidden = false;
    return null;
  },
};

document.addEventListener('submit', async (ev) => {
  const form = ev.target;
  const acao = ACOES[form.dataset.acao];
  if (!acao) return;
  ev.preventDefault();

  const dados = Object.fromEntries(new FormData(form));
  const r = await acao(dados);
  if (!r || !r.ok) return;

  /* deu certo: limpa cadastros e fecha a janela de edição */
  if (form.dataset.acao.startsWith('cadastrar')) {
    form.reset();
    form.querySelector('input')?.focus();
  }
  if (form.dataset.acao === 'alterar-jogador') $('#dlg-jogador').close();
});

document.addEventListener('click', async (ev) => {
  const alvo = ev.target.closest('button');
  if (!alvo) return;

  if (alvo.dataset.aba) return trocarAba(alvo.dataset.aba);
  if (alvo.dataset.equipe) return abrirEquipe(alvo.dataset.equipe);
  if (alvo.dataset.editar) return abrirEdicaoJogador(alvo.dataset.editar);
  if (alvo.hasAttribute('data-fechar')) return alvo.closest('dialog').close();

  if (alvo.dataset.excluir) {
    if (confirm(`Excluir o jogador ${alvo.dataset.excluir}?`))
      await api('POST', '/api/jogadores/excluir', { nick: alvo.dataset.excluir });
    return;
  }
  if (alvo.id === 'btn-excluir-equipe') {
    const id = equipeAberta;
    if (confirm(`Excluir a equipe ${id} · ${nomeEquipe(id)}? Os jogadores dela também serão excluídos.`)) {
      const r = await api('POST', '/api/equipes/excluir', { id });
      if (r && r.ok) $('#dlg-equipe').close();
    }
    return;
  }
  if (alvo.id === 'btn-gerar') {
    await api('POST', '/api/rodada/gerar');
  }
});

$('#filtro-equipe').addEventListener('change', desenharJogadores);
$('#dlg-equipe').addEventListener('close', () => { equipeAberta = null; });

/* ---------------- abas ---------------- */

function trocarAba(nome) {
  document.querySelectorAll('.abas button').forEach((b) => {
    const ativa = b.dataset.aba === nome;
    b.classList.toggle('ativa', ativa);
    b.setAttribute('aria-selected', ativa);
  });
  document.querySelectorAll('.aba').forEach((s) => { s.hidden = s.id !== 'aba-' + nome; });
  history.replaceState(null, '', '#' + nome);
}

/* ---------------- início ---------------- */

const abaInicial = location.hash.slice(1);
trocarAba(['partidas', 'classificacao', 'equipes', 'jogadores'].includes(abaInicial) ? abaInicial : 'partidas');
api('GET', '/api/estado');
