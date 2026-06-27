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

#endif
