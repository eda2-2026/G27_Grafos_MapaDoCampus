#include "grafo.h"

#include <stdio.h>
#include <string.h>

#include "util.h"

#define GRAFO_MAX_VERTICES 256

typedef struct {
    char vertices[GRAFO_MAX_VERTICES][GRAFO_NOME_MAX];
    unsigned char adj[GRAFO_MAX_VERTICES][GRAFO_MAX_VERTICES];
    size_t total_vertices;
} GrafoCampus;

void resultado_bfs_inicializar(ResultadoBfs *resultado) {
    if (resultado == NULL) {
        return;
    }

    resultado->tamanho = 0;
    resultado->distancia = -1;
    resultado->vertices_visitados = 0;
    resultado->arestas_analisadas = 0;
}

static void remover_quebra_linha(char *texto) {
    char *fim = strpbrk(texto, "\r\n");
    if (fim != NULL) {
        *fim = '\0';
    }
}

static int encontrar_vertice(const GrafoCampus *grafo, const char *nome) {
    if (grafo == NULL || nome == NULL) {
        return -1;
    }

    for (size_t i = 0; i < grafo->total_vertices; i++) {
        if (strings_iguais_case_insensitive(grafo->vertices[i], nome)) {
            return (int)i;
        }
    }

    return -1;
}

static int obter_ou_criar_vertice(GrafoCampus *grafo, const char *nome) {
    int existente = encontrar_vertice(grafo, nome);
    if (existente >= 0) {
        return existente;
    }

    if (grafo->total_vertices >= GRAFO_MAX_VERTICES) {
        return -1;
    }

    snprintf(grafo->vertices[grafo->total_vertices], GRAFO_NOME_MAX, "%s", nome);
    grafo->total_vertices++;
    return (int)(grafo->total_vertices - 1);
}

static int adicionar_aresta(GrafoCampus *grafo, const char *origem, const char *destino) {
    int indice_origem = obter_ou_criar_vertice(grafo, origem);
    int indice_destino = obter_ou_criar_vertice(grafo, destino);

    if (indice_origem < 0 || indice_destino < 0) {
        return -1;
    }

    grafo->adj[indice_origem][indice_destino] = 1;
    grafo->adj[indice_destino][indice_origem] = 1;
    return 0;
}

static int carregar_grafo_csv(const char *caminho_csv, GrafoCampus *grafo) {
    if (caminho_csv == NULL || grafo == NULL) {
        return -1;
    }

    memset(grafo, 0, sizeof(*grafo));

    FILE *fp = fopen(caminho_csv, "r");
    if (fp == NULL) {
        return -1;
    }

    char linha[256];
    while (fgets(linha, sizeof(linha), fp) != NULL) {
        remover_quebra_linha(linha);

        if (linha[0] == '\0' || linha[0] == '#') {
            continue;
        }

        if (strncmp(linha, "origem;", 7) == 0) {
            continue;
        }

        char *origem = strtok(linha, ";");
        char *destino = strtok(NULL, ";");

        if (origem == NULL || destino == NULL || origem[0] == '\0' || destino[0] == '\0') {
            continue;
        }

        if (adicionar_aresta(grafo, origem, destino) != 0) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static void reconstruir_caminho(
    const GrafoCampus *grafo,
    const int *anterior,
    int origem,
    int destino,
    ResultadoBfs *resultado
) {
    int pilha[GRAFO_MAX_CAMINHO];
    size_t tamanho = 0;

    for (int atual = destino; atual != -1 && tamanho < GRAFO_MAX_CAMINHO; atual = anterior[atual]) {
        pilha[tamanho++] = atual;
        if (atual == origem) {
            break;
        }
    }

    resultado->tamanho = 0;
    for (size_t i = 0; i < tamanho; i++) {
        int indice = pilha[tamanho - 1 - i];
        snprintf(resultado->caminho[resultado->tamanho], GRAFO_NOME_MAX, "%s", grafo->vertices[indice]);
        resultado->tamanho++;
    }

    resultado->distancia = resultado->tamanho > 0 ? (int)resultado->tamanho - 1 : -1;
}

int grafo_bfs_menor_rota_csv(
    const char *caminho_csv,
    const char *origem,
    const char *destino,
    ResultadoBfs *resultado
) {
    if (caminho_csv == NULL || origem == NULL || destino == NULL || resultado == NULL) {
        return -1;
    }

    resultado_bfs_inicializar(resultado);

    GrafoCampus grafo;
    if (carregar_grafo_csv(caminho_csv, &grafo) != 0) {
        return -1;
    }

    int indice_origem = encontrar_vertice(&grafo, origem);
    int indice_destino = encontrar_vertice(&grafo, destino);

    if (indice_origem < 0 || indice_destino < 0) {
        return -1;
    }

    int fila[GRAFO_MAX_VERTICES];
    int anterior[GRAFO_MAX_VERTICES];
    unsigned char visitado[GRAFO_MAX_VERTICES];
    size_t inicio = 0;
    size_t fim = 0;

    memset(visitado, 0, sizeof(visitado));
    for (size_t i = 0; i < GRAFO_MAX_VERTICES; i++) {
        anterior[i] = -1;
    }

    fila[fim++] = indice_origem;
    visitado[indice_origem] = 1;

    while (inicio < fim) {
        int atual = fila[inicio++];
        resultado->vertices_visitados++;

        if (atual == indice_destino) {
            reconstruir_caminho(&grafo, anterior, indice_origem, indice_destino, resultado);
            return 0;
        }

        for (size_t vizinho = 0; vizinho < grafo.total_vertices; vizinho++) {
            if (!grafo.adj[atual][vizinho]) {
                continue;
            }

            resultado->arestas_analisadas++;

            if (!visitado[vizinho]) {
                visitado[vizinho] = 1;
                anterior[vizinho] = atual;
                fila[fim++] = (int)vizinho;
            }
        }
    }

    return -1;
}

//DFS

static void imprimir_indentacao_dfs(int nivel) {
    for (int i = 0; i < nivel; i++) {
        printf("               ");
    }
}

void executar_dfs_acessibilidade(Grafo *g, int u, int *visitados, int id_interditado, int nivel) {
    visitados[u] = 1;

    imprimir_indentacao_dfs(nivel - 1);
    printf("|_____DFS visit (G, %s)\n", g->nomes[u]);

    NoAdjacencia *adj = g->lista_adj[u];
    while (adj != NULL) {
        int v = adj->destino;

        if (v == id_interditado) {
            imprimir_indentacao_dfs(nivel);
            printf("|_____[INTERDITADO] %s\n", g->nomes[v]);
            imprimir_indentacao_dfs(nivel);
            printf("|___BT___|\n");
        } 
        else if (visitados[v] == 0) {
            executar_dfs_acessibilidade(g, v, visitados, id_interditado, nivel + 1);
        }
        
        adj = adj->prox;
    }
}
