#ifndef ORDENACAO_H
#define ORDENACAO_H

#include <stddef.h>

#include "local.h"

typedef enum {
    ORDENACAO_CAMPO_ID = 0,
    ORDENACAO_CAMPO_NOME,
    ORDENACAO_CAMPO_CAPACIDADE
} OrdenacaoCampo;

typedef enum {
    ORDENACAO_ALGORITMO_QUICKSORT = 0,
    ORDENACAO_ALGORITMO_MERGESORT,
    ORDENACAO_ALGORITMO_HEAPSORT
} OrdenacaoAlgoritmo;

int ordenar_locais(
    Local *locais,
    size_t total,
    OrdenacaoCampo campo,
    int crescente,
    OrdenacaoAlgoritmo algoritmo,
    unsigned long *comparacoes
);

const char *ordenacao_nome_campo(OrdenacaoCampo campo);
const char *ordenacao_nome_algoritmo(OrdenacaoAlgoritmo algoritmo);

#endif
