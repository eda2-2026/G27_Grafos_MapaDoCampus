/*
Funcao no projeto: conectar UI web com a API real em C.
Conteudo: pesquisa textual por filtros, cadastro e renderizacao em cards.
O que faz: separa resultado da busca e lista completa de locais cadastrados.
*/
const API_BASE = "http://localhost:8091";

const formPesquisa = document.getElementById("formPesquisa");
const btnLimparPesquisa = document.getElementById("btnLimparPesquisa");
const formCadastro = document.getElementById("formCadastro");
const formRanking = document.getElementById("formRanking");

const statusPesquisa = document.getElementById("statusPesquisa");
const listaPesquisa = document.getElementById("listaPesquisa");

const statusCadastro = document.getElementById("statusCadastro");
const listaCadastrados = document.getElementById("listaCadastrados");
const statusRanking = document.getElementById("statusRanking");
const listaRanking = document.getElementById("listaRanking");
//MERGE
const formAgenda = document.getElementById("formAgenda");
const statusAgenda = document.getElementById("statusAgenda");
const listaAgenda = document.getElementById("listaAgenda");

function setStatus(elemento, texto, erro = false) {
  elemento.textContent = texto;
  elemento.classList.toggle("error", erro);
}

function escapeHtml(valor) {
  return String(valor)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function extrairNumeroSala(texto) {
  if (!texto) {
    return null;
  }

  const match = texto.match(/^\s*(?:sala\s*)?(\d+)\s*$/i);
  if (!match) {
    return null;
  }
  return match[1];
}

function montarCardLocal(local) {
  const temAr = Number(local.temAr) === 1 ? "Sim" : "Nao";
  const responsavel = local.responsavel || local.professor || "-";
  
  // ==========================================
  // LÓGICA VISUAL DA RELEVÂNCIA (QUICK SORT)
  // ==========================================
  let badgeRelevancia = "";
  if (local.score !== undefined) {
    // Se a API devolveu um score, criamos uma etiqueta visual de destaque
    badgeRelevancia = `<div style="color: #d9534f; font-weight: bold; margin: 8px 0; padding: 4px 8px; background-color: #fdf0ef; border-radius: 6px; display: inline-block;">🔥 Match: ${local.score} pontos</div>`;
  }

  return `
    <article class="local-card">
      <header class="local-card-header">
        <h4>${escapeHtml(local.nome)}</h4>
        <span class="chip">Bloco ${escapeHtml(local.bloco)} - Andar ${escapeHtml(local.andar)}</span>
      </header>
      ${badgeRelevancia}
      <p class="local-card-subtitle">${escapeHtml(local.tipo)} | Capacidade ${escapeHtml(local.capacidade)} | Ar: ${escapeHtml(temAr)}</p>
      <p class="local-card-meta">Materia: ${escapeHtml(local.materia)}</p>
      <p class="local-card-meta">Horario: ${escapeHtml(local.horario || "-")}</p>
      <p class="local-card-meta">Responsavel: ${escapeHtml(responsavel)}</p>
    </article>
  `;
}

function renderListaResultados(elementoLista, locais, vazioMsg) {
  if (!Array.isArray(locais) || locais.length === 0) {
    elementoLista.innerHTML = `<p class="resultado-vazio">${escapeHtml(vazioMsg)}</p>`;
    return;
  }

  elementoLista.innerHTML = locais.map(montarCardLocal).join("");
}

function renderErroEmCard(elementoLista, mensagem) {
  elementoLista.innerHTML = `
    <article class="local-card local-card-error">
      <h4>Nao foi possivel carregar resultados</h4>
      <p class="local-card-meta">${escapeHtml(mensagem)}</p>
    </article>
  `;
}

async function fetchJson(url, options = undefined) {
  const resposta = await fetch(url, options);
  const texto = await resposta.text();

  let dados;
  try {
    dados = JSON.parse(texto);
  } catch {
    throw new Error(`Resposta nao-JSON da API (${resposta.status})`);
  }

  if (!resposta.ok) {
    const msg = dados && dados.erro ? dados.erro : `Erro HTTP ${resposta.status}`;
    throw new Error(msg);
  }

  return dados;
}

function montarQueryPesquisa() {
  const params = new URLSearchParams();

  const idDigitado = document.getElementById("pId").value.trim();
  const nomeOriginal = document.getElementById("pNome").value.trim();
  const materia = document.getElementById("pMateria").value.trim();
  const bloco = document.getElementById("pBloco").value.trim();
  const andar = document.getElementById("pAndar").value.trim();
  const tipo = document.getElementById("pTipo").value.trim();
  const responsavel = document.getElementById("pResponsavel").value.trim();
  const horario = document.getElementById("pHorario").value.trim();
  const temAr = document.getElementById("pTemAr").value;
  const capacidade = document.getElementById("pCapacidade").value.trim();
  const ordenarPor = document.getElementById("pOrdenarPor").value;
  const algoritmoOrdenacao = document.getElementById("pAlgoritmoOrdenacao").value;
  const ordem = document.getElementById("pOrdem").value;

  let idBusca = idDigitado;
  let nome = nomeOriginal;

  if (!idBusca) {
    const salaExtraida = extrairNumeroSala(nomeOriginal);
    if (salaExtraida) {
      idBusca = salaExtraida;
      nome = "";
    }
  }

  if (idBusca) params.set("id", idBusca);
  if (nome) params.set("nome", nome);
  if (materia) params.set("materia", materia);
  if (bloco) params.set("bloco", bloco);
  if (andar) params.set("andar", andar);
  if (tipo) params.set("tipo", tipo);
  if (responsavel) {
    params.set("responsavel", responsavel);
    params.set("professor", responsavel);
  }
  if (horario) params.set("horario", horario);
  if (temAr !== "") params.set("temAr", temAr);
  if (capacidade) params.set("capacidadeMin", capacidade);
  if (ordenarPor) {
    params.set("ordenarPor", ordenarPor);
    if (algoritmoOrdenacao) params.set("algoritmoOrdenacao", algoritmoOrdenacao);
    if (ordem) params.set("ordem", ordem);
  }

  return params;
}

async function pesquisarLocais() {
  const params = montarQueryPesquisa();
  const sufixo = params.toString();

  if (!sufixo) {
    renderListaResultados(listaPesquisa, [], "Digite ao menos um filtro para pesquisar.");
    setStatus(statusPesquisa, "Digite ao menos um filtro (ou numero da sala) para pesquisar.");
    return;
  }

  const dados = await fetchJson(`${API_BASE}/api/busca?${sufixo}`);
  renderListaResultados(listaPesquisa, dados.locais || [], "Nenhum local encontrado com esses filtros.");

  const ordenacao = dados.ordenacao || null;
  const ordenacaoTexto = ordenacao
    ? `${ordenacao.algoritmo || "padrao"}(${ordenacao.campo || "padrao"},${ordenacao.ordem || "asc"})`
    : "padrao";
  setStatus(
    statusPesquisa,
    `Metodo usado: ${dados.metodoUsado || "algoritmo"} | Consulta: ${dados.consulta || "auto"} | Ordenacao=${ordenacaoTexto} | Total=${dados.total} | ComparacoesBusca=${dados.comparacoes ?? "-"} | ComparacoesOrdenacao=${dados.comparacoesOrdenacao ?? "-"}.`
  );
}

async function carregarLocaisCadastrados() {
  const dados = await fetchJson(`${API_BASE}/api/locais`);
  renderListaResultados(listaCadastrados, dados.locais || [], "Nenhum local cadastrado.");
  setStatus(statusCadastro, `Total de locais cadastrados: ${dados.total}.`);
}

async function carregarRankingCapacidade() {
  const top = document.getElementById("rTop").value.trim();
  const params = new URLSearchParams();
  if (top) {
    params.set("top", top);
  }

  const sufixo = params.toString();
  const url = sufixo
    ? `${API_BASE}/api/ranking/capacidade?${sufixo}`
    : `${API_BASE}/api/ranking/capacidade`;

  const dados = await fetchJson(url);
  renderListaResultados(listaRanking, dados.locais || [], "Sem resultados para esse ranking.");
  setStatus(
    statusRanking,
    `Metodo usado: ${dados.metodoUsado || "heapsort"} | Total=${dados.total ?? 0} | Comparacoes=${dados.comparacoes ?? "-"}.`
  );
}

formPesquisa.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(statusPesquisa, "Pesquisando locais...");

  try {
    await pesquisarLocais();
  } catch (erro) {
    setStatus(statusPesquisa, `Falha na pesquisa: ${erro.message}`, true);
    renderErroEmCard(listaPesquisa, erro.message);
  }
});

btnLimparPesquisa.addEventListener("click", () => {
  formPesquisa.reset();
  renderListaResultados(listaPesquisa, [], "Digite ao menos um filtro para pesquisar.");
  setStatus(statusPesquisa, "Filtros limpos. Preencha os campos e clique em Pesquisar.");
});

formCadastro.addEventListener("submit", async (event) => {
  event.preventDefault();

  const payload = new URLSearchParams({
    id: document.getElementById("cId").value.trim(),
    nome: document.getElementById("cNome").value.trim(),
    bloco: document.getElementById("cBloco").value.trim(),
    andar: document.getElementById("cAndar").value.trim(),
    tipo: document.getElementById("cTipo").value.trim(),
    capacidade: document.getElementById("cCapacidade").value.trim(),
    temAr: document.getElementById("cTemAr").value,
    responsavel: document.getElementById("cResponsavel").value.trim(),
    professor: document.getElementById("cResponsavel").value.trim(),
    materia: document.getElementById("cMateria").value.trim(),
    horario: document.getElementById("cHorario").value.trim(),
  });

  setStatus(statusCadastro, "Cadastrando local...");

  try {
    const dados = await fetchJson(`${API_BASE}/api/locais`, {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
      },
      body: payload.toString(),
    });

    const metodo = dados.metodoInsercao || "insertion_sort_id";
    const movimentacoes = dados.movimentacoes ?? "-";
    setStatus(
      statusCadastro,
      `${dados.mensagem}. ID criado: ${dados.id}. Metodo: ${metodo}. Movimentacoes=${movimentacoes}.`
    );
    formCadastro.reset();
    await carregarLocaisCadastrados();
  } catch (erro) {
    setStatus(statusCadastro, `Falha ao cadastrar local: ${erro.message}`, true);
    renderErroEmCard(listaCadastrados, erro.message);
  }
});

formRanking.addEventListener("submit", async (event) => {
  event.preventDefault();
  setStatus(statusRanking, "Gerando ranking...");

  try {
    await carregarRankingCapacidade();
  } catch (erro) {
    setStatus(statusRanking, `Falha ao gerar ranking: ${erro.message}`, true);
    renderErroEmCard(listaRanking, erro.message);
  }
});

async function carregarAgendaProfessor() {
  const professorDigitado = document.getElementById("aResponsavel").value.trim();
  
  if (!professorDigitado) {
    renderListaResultados(listaAgenda, [], "Digite o nome do professor.");
    setStatus(statusAgenda, "O campo responsavel nao pode estar vazio.", true);
    return;
  }

  const url = `${API_BASE}/api/agenda?responsavel=${encodeURIComponent(professorDigitado)}`;
  const dados = await fetchJson(url);
  
  renderListaResultados(listaAgenda, dados.dados || [], "Nenhuma sala encontrada para este professor.");
  
  setStatus(
    statusAgenda,
    `Metodo usado: Merge Sort Iterativo | Aulas encontradas: ${dados.total ?? 0}`
  );
}

formAgenda.addEventListener("submit", async (event) => {
  event.preventDefault(); // Evita recarregar a pagina
  setStatus(statusAgenda, "Carregando agenda com Merge Sort...");

  try {
    await carregarAgendaProfessor();
  } catch (erro) {
    setStatus(statusAgenda, `Falha ao carregar agenda: ${erro.message}`, true);
    renderErroEmCard(listaAgenda, erro.message);
  }
});



(async function iniciarTela() {
  renderListaResultados(listaPesquisa, [], "Digite ao menos um filtro para pesquisar.");
  renderListaResultados(listaRanking, [], "Clique em gerar ranking para ver as maiores salas.");
  renderListaResultados(listaAgenda, [], "Aguardando busca pela agenda do professor.");
  try {
    await carregarLocaisCadastrados();
    await carregarRankingCapacidade();
  } catch (erro) {
    setStatus(
      statusCadastro,
      [
        "Nao foi possivel conectar na API.",
        "Inicie o backend com: make run-api",
        `Detalhe: ${erro.message}`,
      ].join("\n"),
      true
    );
    renderErroEmCard(listaCadastrados, erro.message);
    setStatus(statusRanking, `Falha ao carregar ranking: ${erro.message}`, true);
    renderErroEmCard(listaRanking, erro.message);
  }
})();
