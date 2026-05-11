#include "ordenacao.h"
#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int comparar_texto_case_insensitive(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        unsigned char ca = (unsigned char)tolower((unsigned char)a[i]);
        unsigned char cb = (unsigned char)tolower((unsigned char)b[i]);
        if (ca < cb) {
            return -1;
        }
        if (ca > cb) {
            return 1;
        }
        i++;
    }

    if (a[i] == '\0' && b[i] == '\0') {
        return 0;
    }
    return a[i] == '\0' ? -1 : 1;
}

static int comparar_locais(const Local *a, const Local *b, OrdenacaoCampo campo) {
    int comparacao = 0;
    switch (campo) {
        case ORDENACAO_CAMPO_ID:
            if (a->id < b->id) {
                comparacao = -1;
            } else if (a->id > b->id) {
                comparacao = 1;
            }
            break;
        case ORDENACAO_CAMPO_NOME:
            comparacao = comparar_texto_case_insensitive(a->nome, b->nome);
            break;
        case ORDENACAO_CAMPO_CAPACIDADE:
            if (a->capacidade < b->capacidade) {
                comparacao = -1;
            } else if (a->capacidade > b->capacidade) {
                comparacao = 1;
            }
            break;
        default:
            break;
    }

    if (comparacao == 0) {
        if (a->id < b->id) {
            return -1;
        }
        if (a->id > b->id) {
            return 1;
        }
    }

    return comparacao;
}

static int comparar_para_ordem(
    const Local *a,
    const Local *b,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    if (comparacoes != NULL) {
        (*comparacoes)++;
    }

    int valor = comparar_locais(a, b, campo);
    return crescente ? valor : -valor;
}

static void trocar_locais(Local *a, Local *b) {
    if (a == b) {
        return;
    }
    Local tmp = *a;
    *a = *b;
    *b = tmp;
}

static size_t quicksort_particionar(
    Local *locais,
    size_t inicio,
    size_t fim,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    Local pivo = locais[fim];
    size_t i = inicio;

    for (size_t j = inicio; j < fim; j++) {
        if (comparar_para_ordem(&locais[j], &pivo, campo, crescente, comparacoes) <= 0) {
            trocar_locais(&locais[i], &locais[j]);
            i++;
        }
    }

    trocar_locais(&locais[i], &locais[fim]);
    return i;
}

static void quicksort_rec(
    Local *locais,
    size_t inicio,
    size_t fim,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    if (inicio >= fim) {
        return;
    }

    size_t pivo = quicksort_particionar(locais, inicio, fim, campo, crescente, comparacoes);

    if (pivo > 0) {
        quicksort_rec(locais, inicio, pivo - 1, campo, crescente, comparacoes);
    }
    quicksort_rec(locais, pivo + 1, fim, campo, crescente, comparacoes);
}

static int mergesort_mesclar(
    Local *locais,
    Local *temp,
    size_t inicio,
    size_t meio,
    size_t fim,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    size_t i = inicio;
    size_t j = meio;
    size_t k = inicio;

    while (i < meio && j < fim) {
        if (comparar_para_ordem(&locais[i], &locais[j], campo, crescente, comparacoes) <= 0) {
            temp[k++] = locais[i++];
        } else {
            temp[k++] = locais[j++];
        }
    }

    while (i < meio) {
        temp[k++] = locais[i++];
    }
    while (j < fim) {
        temp[k++] = locais[j++];
    }

    memcpy(&locais[inicio], &temp[inicio], (fim - inicio) * sizeof(Local));
    return 0;
}

static int mergesort_rec(
    Local *locais,
    Local *temp,
    size_t inicio,
    size_t fim,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    if (fim - inicio <= 1) {
        return 0;
    }

    size_t meio = inicio + (fim - inicio) / 2;
    if (mergesort_rec(locais, temp, inicio, meio, campo, crescente, comparacoes) != 0) {
        return -1;
    }
    if (mergesort_rec(locais, temp, meio, fim, campo, crescente, comparacoes) != 0) {
        return -1;
    }
    return mergesort_mesclar(locais, temp, inicio, meio, fim, campo, crescente, comparacoes);
}

static void heapsort_heapify(
    Local *locais,
    size_t tamanho_heap,
    size_t raiz,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    size_t maior_ou_menor = raiz;
    size_t esquerda = 2 * raiz + 1;
    size_t direita = 2 * raiz + 2;

    if (esquerda < tamanho_heap &&
        comparar_para_ordem(&locais[esquerda], &locais[maior_ou_menor], campo, crescente, comparacoes) > 0) {
        maior_ou_menor = esquerda;
    }

    if (direita < tamanho_heap &&
        comparar_para_ordem(&locais[direita], &locais[maior_ou_menor], campo, crescente, comparacoes) > 0) {
        maior_ou_menor = direita;
    }

    if (maior_ou_menor != raiz) {
        trocar_locais(&locais[raiz], &locais[maior_ou_menor]);
        heapsort_heapify(locais, tamanho_heap, maior_ou_menor, campo, crescente, comparacoes);
    }
}

static void heapsort_ordenar(
    Local *locais,
    size_t total,
    OrdenacaoCampo campo,
    int crescente,
    unsigned long *comparacoes
) {
    if (total < 2) {
        return;
    }

    for (size_t i = total / 2; i > 0; i--) {
        heapsort_heapify(locais, total, i - 1, campo, crescente, comparacoes);
    }

    for (size_t i = total; i > 1; i--) {
        trocar_locais(&locais[0], &locais[i - 1]);
        heapsort_heapify(locais, i - 1, 0, campo, crescente, comparacoes);
    }
}

int ordenar_locais(
    Local *locais,
    size_t total,
    OrdenacaoCampo campo,
    int crescente,
    OrdenacaoAlgoritmo algoritmo,
    unsigned long *comparacoes
) {
    if (locais == NULL) {
        return -1;
    }

    if (comparacoes != NULL) {
        *comparacoes = 0;
    }

    if (total < 2) {
        return 0;
    }

    if (algoritmo == ORDENACAO_ALGORITMO_QUICKSORT) {
        quicksort_rec(locais, 0, total - 1, campo, crescente, comparacoes);
        return 0;
    }

    if (algoritmo == ORDENACAO_ALGORITMO_MERGESORT) {
        Local *temp = (Local *)malloc(total * sizeof(Local));
        if (temp == NULL) {
            return -1;
        }
        int status = mergesort_rec(locais, temp, 0, total, campo, crescente, comparacoes);
        free(temp);
        return status;
    }

    if (algoritmo == ORDENACAO_ALGORITMO_HEAPSORT) {
        heapsort_ordenar(locais, total, campo, crescente, comparacoes);
        return 0;
    }

    return -1;
}

const char *ordenacao_nome_campo(OrdenacaoCampo campo) {
    switch (campo) {
        case ORDENACAO_CAMPO_ID:
            return "id";
        case ORDENACAO_CAMPO_NOME:
            return "nome";
        case ORDENACAO_CAMPO_CAPACIDADE:
            return "capacidade";
        default:
            return "id";
    }
}

const char *ordenacao_nome_algoritmo(OrdenacaoAlgoritmo algoritmo) {
    switch (algoritmo) {
        case ORDENACAO_ALGORITMO_QUICKSORT:
            return "quicksort";
        case ORDENACAO_ALGORITMO_MERGESORT:
            return "mergesort";
        case ORDENACAO_ALGORITMO_HEAPSORT:
            return "heapsort";
        default:
            return "quicksort";
    }
}


// MERGE SORT ITERATIVO


static int comparar_agenda(const Local *a, const Local *b) {
    int cmp_horario = strcmp(a->horario, b->horario);
    if (cmp_horario != 0) {
        return cmp_horario;
    }
    return strcmp(a->nome, b->nome);
}

static void merge_agenda(Local *locais, Local *temp, int inicio, int meio, int fim) {
    int i = inicio;
    int j = meio;
    int k = inicio;

    while (i < meio && j < fim) {
        if (comparar_agenda(&locais[i], &locais[j]) <= 0) {
            temp[k] = locais[i];
            i++;
        } else {
            temp[k] = locais[j];
            j++;
        }
        k++;
    }

    while (i < meio) {
        temp[k] = locais[i];
        i++;
        k++;
    }
    while (j < fim) {
        temp[k] = locais[j];
        j++;
        k++;
    }

    for (int p = inicio; p < fim; p++) {
        locais[p] = temp[p];
    }
}

static int min_agenda(int a, int b) {
    return (a < b) ? a : b;
}

void mergesort_agenda_iterativo(Local *locais, int total) {
    Local *temp = (Local *)malloc(total * sizeof(Local));
    if (temp == NULL) {
        return;
    }

    for (int gap = 1; gap < total; gap *= 2) {
        for (int inicio = 0; inicio < total; inicio += 2 * gap) {
            int meio = min_agenda(inicio + gap, total);
            int fim = min_agenda(inicio + 2 * gap, total);
            merge_agenda(locais, temp, inicio, meio, fim);
        }
    }

    free(temp);
}

//QUICK SORT - RELEVANCIA

int calcular_relevancia(const Local *l, const FiltroLocal *f) {
    int score = 0;

    if (f->nome != NULL && strings_iguais_case_insensitive(l->nome, f->nome)) {
        score += 50;
    }
    if (f->bloco != NULL && strings_iguais_case_insensitive(l->bloco, f->bloco)) {
        score += 30;
    }
    if (f->materia != NULL && strings_iguais_case_insensitive(l->materia, f->materia)) {
        score += 20;
    }
    if (f->horario != NULL && strings_iguais_case_insensitive(l->horario, f->horario)) {
        score += 10;
    }

    return score;
}

static void trocar_relevancia(LocalRelevancia *a, LocalRelevancia *b) {
    LocalRelevancia temp = *a;
    *a = *b;
    *b = temp;
}

static int quicksort_particionar_relevancia(LocalRelevancia *vetor, int inicio, int fim) {
    int pontuacao_pivo = vetor[fim].score;
    int i = inicio - 1;

    for (int j = inicio; j < fim; j++) {
        if (vetor[j].score >= pontuacao_pivo) {
            i++;
            trocar_relevancia(&vetor[i], &vetor[j]);
        }
    }
    
    trocar_relevancia(&vetor[i + 1], &vetor[fim]);
    return i + 1;
}

static void quicksort_rec_relevancia(LocalRelevancia *vetor, int inicio, int fim) {
    if (inicio < fim) {
        int indice_pivo = quicksort_particionar_relevancia(vetor, inicio, fim);
        quicksort_rec_relevancia(vetor, inicio, indice_pivo - 1);
        quicksort_rec_relevancia(vetor, indice_pivo + 1, fim);
    }
}

void ordenar_por_relevancia(LocalRelevancia *vetor, int total) {
    if (vetor == NULL || total < 2) {
        return; 
    }
    quicksort_rec_relevancia(vetor, 0, total - 1);
}