#ifndef ARVORE_AVL_H
#define ARVORE_AVL_H

#include <stddef.h>

#include "local.h"

// Limite de salas retornadas quando varias salas possuem a mesma capacidade ideal.
#define AVL_MAX_SUGESTOES 128

// Guarda o resultado da consulta AVL e as metricas usadas para explicar o algoritmo.
typedef struct {
    size_t indices[AVL_MAX_SUGESTOES];
    size_t quantidade;
    size_t total_indexados;
    unsigned long comparacoes;
    unsigned long rotacoes;
    int altura_arvore;
    int capacidade_encontrada;
} ResultadoAvlCapacidade;

// Zera todos os campos do resultado antes de montar e consultar a arvore.
void resultado_avl_inicializar(ResultadoAvlCapacidade *resultado);

// Monta uma AVL por capacidade e busca a menor sala que atende capacidadeMin.
int avl_sugerir_por_capacidade(
    const Local *locais,
    size_t total,
    const FiltroLocal *filtro,
    ResultadoAvlCapacidade *resultado
);

#endif
