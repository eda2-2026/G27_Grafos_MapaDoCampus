#ifndef DATASET_H
#define DATASET_H

#include <stddef.h>
#include "local.h"

size_t carregar_dataset_exemplo(Local *destino, size_t capacidade);
void ordenar_locais_por_id(Local *locais, size_t total);
int inserir_local_ordenado_por_id(
    Local *locais,
    size_t *total,
    size_t capacidade,
    const Local *novo_local,
    unsigned long *movimentacoes
);

#endif
