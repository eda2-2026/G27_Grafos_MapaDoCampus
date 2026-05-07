
#ifndef BUSCA_HASH_H
#define BUSCA_HASH_H

#include "stddef.h"
#include "util.h"


typedef struct ListaEnc_hash {
    const Local *dados_local;
    struct ListaEnc_hash *proximo;
} ListaEnc_hash;

typedef struct {
    ListaEnc_hash **inicio; 
    size_t tamanho_total;
} TabelaHash;

TabelaHash* criar_tabela_hash(size_t tamanho);
void destruir_tabela_hash(TabelaHash *tabela);
void inserir_hash(TabelaHash *tabela, const Local *local_campus);
const Local* buscar_hash(TabelaHash *tabela, const char *nome_buscado);

#endif