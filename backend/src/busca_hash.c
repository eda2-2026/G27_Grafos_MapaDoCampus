#include "busca_hash.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static size_t calcular_hash(const char *texto, size_t tabela_tamanho) {
    if (texto == NULL || tabela_tamanho == 0) {
        return 0;
    }
    size_t letras_soma = 0;  
    for (size_t i = 0; texto[i] != '\0'; i++) {

        letras_soma = letras_soma + (unsigned char) tolower ((unsigned char) texto[i]);
    }

    return letras_soma % tabela_tamanho;
}

TabelaHash* criar_tabela_hash(size_t tamanho) {
    if (tamanho == 0) {
        return NULL;
    }
    TabelaHash *tabela = (TabelaHash *) malloc (sizeof(TabelaHash));
    
    if (tabela == NULL) {
        return NULL;
    }

    tabela->inicio = (ListaEnc_hash **) calloc (tamanho, sizeof(ListaEnc_hash *));
    if(tabela->inicio == NULL){
        free (tabela);
        return NULL;
    }

    tabela->tamanho_total = tamanho;
    return tabela;
}

void destruir_tabela_hash(TabelaHash *tabela) {
    if (tabela == NULL) {
        return;
    }

    if (tabela->inicio != NULL) {
        for (size_t i = 0; i < tabela->tamanho_total; i++) {
            ListaEnc_hash *atual = tabela->inicio[i];
            while (atual != NULL) {
                ListaEnc_hash *proximo = atual->proximo;
                free(atual);
                atual = proximo;
            }
        }
        free(tabela->inicio);
    }

    free(tabela);
}

void inserir_hash(TabelaHash *tabela, const Local *local_campus) {
    if (tabela == NULL || local_campus == NULL) return;

    size_t indice_hash = calcular_hash(local_campus->nome, tabela->tamanho_total);

    ListaEnc_hash *novo_local = (ListaEnc_hash *)malloc(sizeof(ListaEnc_hash));
    if (novo_local == NULL) return; // verifica memoria
    
    novo_local->dados_local = local_campus;

    novo_local->proximo = tabela->inicio[indice_hash];

    tabela->inicio[indice_hash] = novo_local;
}


static int cadastro_vs_busca(const char *local_cadastrado, const char *local_buscado) {
    int i = 0;

    while (local_cadastrado[i] != '\0' && local_buscado[i] != '\0') {
        

        unsigned char letra_cadastro = (unsigned char)tolower((unsigned char)local_cadastrado[i]);
        unsigned char letra_busca = (unsigned char)tolower((unsigned char)local_buscado[i]);
        
        if (letra_cadastro != letra_busca) {
            return 0; 
        }

        i++; 
    }

    if (local_cadastrado[i] == '\0' && local_buscado[i] == '\0') {
        return 1; 
    }

    return 0; 
}

const Local* buscar_hash(TabelaHash *tabela, const char *nome_buscado) {
    if (tabela == NULL || nome_buscado == NULL) return NULL;

    size_t indice_hash = calcular_hash(nome_buscado, tabela->tamanho_total);

    ListaEnc_hash *local_em_verificacao = tabela->inicio[indice_hash];

    while (local_em_verificacao != NULL) {
        if (cadastro_vs_busca(local_em_verificacao->dados_local->nome, nome_buscado) == 1) {
            return local_em_verificacao->dados_local; 
        }

        local_em_verificacao = local_em_verificacao->proximo;
    }

    return NULL; 
}