#ifndef grafo_h
#define grafo_h

#include <stddef.h>

#define GRAFO_NOME_MAX 100
#define GRAFO_MAX_CAMINHO 256

typedef struct {
    char caminho[GRAFO_MAX_CAMINHO][GRAFO_NOME_MAX];
    size_t tamanho;
    int distancia;
    unsigned long vertices_visitados;
    unsigned long arestas_analisadas;
} ResultadoBfs;

void resultado_bfs_inicializar(ResultadoBfs *resultado);

int grafo_bfs_menor_rota_csv(
    const char *caminho_csv,
    const char *origem,
    const char *destino,
    ResultadoBfs *resultado
);

//DFS
void executar_dfs_acessibilidade(Grafo *g, int u, int *visitados, int id_interditado, int nivel);
int testar_acessibilidade_campus(Grafo *g, int id_origem, int id_interditado, int *ids_isolados, int capacidade_max);

#endif
