# Mapa de Campus

Número da Lista: Trabalho 4 - Grafos <br>
Conteúdo da Disciplina: Grafos <br>

## Alunas
|Matrícula | Aluno |
| -- | -- |
| 231035722  | Marina Agostini Galdi |
| 241036142  | Júlia Gabriella Ferreira Siqueira |

## Sobre
O **Mapa de Campus** é um serviço projetado para gerenciar e consultar o catálogo de locais de um campus universitário (salas, laboratórios, auditórios, etc.). O principal objetivo deste projeto é aplicar estruturas de dados de alto desempenho na prática, conectando um backend nativo em C a uma interface Web de pesquisas, filtros e cadastros dinâmicos.

Nesta **Entrega 4**, o sistema evoluiu para incorporar **Grafos** ao mapa do campus. A base de locais continua sendo consultada e filtrada pelas estruturas das entregas anteriores, mas agora o campus também é modelado como uma rede de conexões entre salas, corredores, escadas, halls e áreas externas. Com isso, o sistema passa a responder perguntas de deslocamento e acessibilidade.

- **Mapa do Campus como Grafo:** As conexões físicas estão em `backend/data/conexoes.csv`. Cada local é tratado como vértice e cada ligação como aresta não direcionada. O backend carrega esse arquivo em uma matriz de adjacência para executar percursos sobre o campus.

- **Sugestão de Sala com Menor Rota (AVL + BFS):** O usuário informa a quantidade de alunos e o local de origem. Primeiro, a AVL encontra salas suficientes pela estratégia *best fit*. Em seguida, o backend usa **BFS** no grafo do campus para escolher uma sala alcançável com a menor quantidade de conexões e retornar o caminho até ela.

- **Simulador de Interdições com DFS:** O usuário informa uma origem e um ponto interditado. O backend executa **DFS** evitando o vértice bloqueado e retorna quais locais ficam isolados a partir da origem escolhida.

O projeto também mantém as otimizações das entregas anteriores:
- **Buscas (O(1) a O(log n)):** Tabela Hash para nomes exatos, Busca Binária, Sequencial Indexada e Interpolação para dados numéricos.
- **Ordenação Dinâmica:** Heap Sort (ranking de capacidade), Merge Sort (agenda estável do professor), Quick Sort (busca por relevância) e Insertion Sort com sentinela (cadastro físico).
- **Árvores Balanceadas:** AVL para sugestão por capacidade e Árvore Vermelho-Preta para validação e remoção de agendamentos sem conflito.

## Casos de Uso da Entrega 4

**1. Melhor sala com menor caminho — AVL + BFS**
> O usuário informa a origem e a quantidade de alunos. A AVL seleciona salas com capacidade suficiente e o BFS percorre o grafo do campus por níveis para devolver a rota com menos conexões até a sala escolhida.
> **Tela:** Seção "Sugestão inteligente de sala".

**2. Simulação de interdição — DFS**
> O usuário informa onde está e qual ponto do campus foi bloqueado. A DFS visita os vértices alcançáveis sem passar pelo local interditado e lista os ambientes que ficaram inacessíveis.
> **Tela:** Seção "Simulador de Interdições (Acessibilidade)".

**3. Grafo persistido em CSV**
> O arquivo `backend/data/conexoes.csv` descreve o mapa usado pelos percursos. Isso permite alterar a topologia do campus sem mexer na lógica dos algoritmos.
> **Arquivo:** `backend/data/conexoes.csv`.

**4. Funcionalidades mantidas das entregas anteriores**
> Buscas, ordenações, AVL e Árvore Vermelho-Preta continuam disponíveis para consulta, ranking, cadastro, agenda e validação de conflitos.
> **Telas:** Pesquisa de locais, ranking, cadastro e agenda do professor.

### 📝 Nota de Arquitetura e Implementação

> **Observação para a Avaliação:** Para fins de organização, as lógicas foram modularizadas:
>
> - `grafo.c` / `grafo.h`: Contém a carga do grafo a partir do CSV, a matriz de adjacência, o BFS de menor rota e a DFS de acessibilidade com local interditado.
> - `arvore_avl.c` / `arvore_avl.h`: Contém a lógica de rotações por Fator de Balanceamento e a busca otimizada *lower bound* para a sugestão de capacidades.
> - `arvore_vp.c` / `arvore_vp.h`: Contém o motor do nó sentinela (`T_nil`), o reequilíbrio por cores (Tio Vermelho/Preto na inserção) e a resolução do complexo caso do Duplo Preto na remoção de agendamentos.

## Screenshots

**1. Interface: Sugestão de Sala com Rota (AVL + BFS)**
`docs/Print1_SugestaoSala_Rota_BFS.PNG`

**2. Interface: Caminho Retornado pelo BFS**
`docs/Print2_Caminho_BFS.PNG`

**3. Interface: Simulador de Interdições antes da Consulta**
`docs/Print3_Simulador_Interdicoes_DFS.PNG`

**4. Interface: Resultado da DFS com Locais Isolados**
`docs/Print4_Resultado_DFS_Isolados.PNG`

**5. Estrutura do Código: Grafo, Matriz de Adjacência e Carga do CSV**
`docs/Print5_Grafo_MatrizAdjacencia_CSV.PNG`

**6. Estrutura do Código: BFS de Menor Rota**
`docs/Print6_BFS_MenorRota.PNG`

**7. Estrutura do Código: DFS de Acessibilidade**
`docs/Print7_DFS_Acessibilidade.PNG`

**8. Servidores em Execução**
`docs/Print8_Terminais_Entrega4.PNG`

## Instalação 
Linguagem: C (Backend) e HTML/JS/CSS (Frontend)<br>
Framework: Nenhum (Sockets nativos em C)<br>

**Pré-requisitos:**
* Compilador `gcc` (C11)
* `make`
* `python3` (necessário apenas para subir o servidor local do frontend)

## Uso 
Para rodar a aplicação completa, você precisará iniciar o backend e o frontend em **dois terminais diferentes**.

**Terminal 1 (Iniciando o Backend em C):**
```bash
make run-api
```

**Terminal 2 (Iniciando o Frontend):**
```bash
cd frontend
python3 -m http.server 5500
```

Após rodar os dois comandos, abra o seu navegador e acesse: `http://localhost:5500`.

**Documentação da API Disponível:**
* `GET /api/busca`: Busca com filtros (ex: `id`, `nome`, `bloco`, `andar`, `tipo`, `responsavel`, `materia`, `horario`, `temAr`, `capacidadeMin`). O sistema decide automaticamente qual algoritmo utilizar com base no parâmetro fornecido.
  * Ordenação opcional: `ordenarPor=id|nome|capacidade|relevancia`, `algoritmoOrdenacao=quicksort|mergesort|heapsort|insertionsort`, `ordem=asc|desc`.
* `GET /api/locais`: Lista todos os locais cadastrados (também aceita os parâmetros de ordenação opcionais).
* `POST /api/locais`: Cadastro de novo local (form urlencoded). Antes de inserir, o backend constrói uma **Árvore Vermelho-Preta** com os agendamentos existentes da sala e valida se há conflito de horário.
* `DELETE /api/locais/:id`: Exclusão de agendamento. O motor da **Árvore Vermelho-Preta** executa as rotações de Duplo Preto se necessário e atualiza o CSV.
* `GET /api/ranking/capacidade`: Ranking de maiores salas por capacidade usando **Heap Sort**.
  * Parâmetro opcional: `top` (ex: `GET /api/ranking/capacidade?top=10`).
* `GET /api/sugestao/avl`: Sugestão de sala por capacidade usando **Árvore AVL**.
  * Parâmetro obrigatório: `capacidadeMin` (ex: `GET /api/sugestao/avl?capacidadeMin=73`).
  * Parâmetros opcionais: `bloco`, `horario`, `temAr`, `andar`, `tipo`, `responsavel`, `materia`.
  * A resposta inclui métricas: `comparacoes`, `rotacoes`, `alturaArvore`, `totalIndexados` e `capacidadeEncontrada`.
* `GET /api/sugestao/avl-bfs`: Sugestão de sala com menor rota usando **Árvore AVL + BFS**.
  * Parâmetros obrigatórios: `capacidadeMin` e `origem` (ex: `GET /api/sugestao/avl-bfs?capacidadeMin=73&origem=Biblioteca&bloco=UAC`).
  * Parâmetros opcionais: `bloco`, `horario`, `temAr`, `andar`, `tipo`, `responsavel`, `materia`.
  * A resposta inclui métricas da AVL e do BFS: `comparacoesAvl`, `rotacoesAvl`, `alturaArvore`, `distanciaBfs`, `verticesVisitadosBfs`, `arestasAnalisadasBfs` e `caminho`.
* `GET /api/acessibilidade`: Simulação de interdição usando **DFS** no grafo do campus.
  * Parâmetros obrigatórios: `origem` e `interditado` (ex: `GET /api/acessibilidade?origem=Biblioteca&interditado=Escada%20UAC`).
  * A resposta informa `totalIsolados` e `locaisIsolados`, indicando quais pontos não são alcançáveis a partir da origem sem passar pelo local interditado.

*Exemplo de requisição de busca nativa via terminal (cURL):*
```bash
curl "http://localhost:8091/api/busca?bloco=UAC&capacidadeMin=40"
```

## Outros 
**Estrutura de Pastas do Projeto:**
* `backend/include`: Contratos compartilhados (structs e assinaturas).
* `backend/src`: Implementações em C (grafos, métodos de busca clássicos, Busca Hash, algoritmos de ordenação, árvores e API).
* `backend/data`: Banco local em CSV com as salas (`locais.csv`) e conexões do grafo do campus (`conexoes.csv`).
* `frontend`: Interface de pesquisa, filtros, cadastro e ranking conectada à API.
* `docs`: Setup e organização da equipe (veja o guia completo em [docs/SETUP.md](docs/SETUP.md)).

**Destaques da Arquitetura:**
* **Grafo do Campus:** `conexoes.csv` descreve vértices e arestas; `grafo.c` carrega essa estrutura e executa BFS e DFS.
* **BFS para Menor Rota:** A sugestão AVL + BFS combina capacidade mínima com caminho de menor número de conexões.
* **DFS para Acessibilidade:** A simulação de interdição percorre o grafo evitando o local bloqueado e identifica pontos isolados.
* **Modularidade:** As lógicas de Grafo, AVL e Árvore Vermelho-Preta foram isoladas em módulos próprios, separados da infraestrutura HTTP.
* **Roteamento de Ordenação:** Insertion Sort para inserção pontual, Quick Sort para buscas gerais, Merge Sort para ordenações estáveis e Heap Sort para rankings.
* **Roteamento de Busca:** O sistema seleciona automaticamente entre Tabela Hash, Busca Binária, Busca por Interpolação e Busca Sequencial Indexada conforme o parâmetro recebido.
* **Case-Insensitive Search na Hash:** Varredura caractere por caractere com conversão para *lowercase* em tempo de execução, permitindo encontrar locais independentemente da formatação textual da query.

## Apresentação em Vídeo
**Vídeo de Apresentação e Explicação do Código da Entrega 4:**

[![Explicação do Código](https://img.youtube.com/vi/kG7OfFlFz8E/0.jpg)](https://youtu.be/kG7OfFlFz8E)
