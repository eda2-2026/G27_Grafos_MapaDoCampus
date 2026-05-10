#include "ordenacao.h"

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

/*MERGE SORT*/

int comparar_agenda(const Local *a, const Local *b) {
    //horários das duas salas
    int compara_horario = strcmp(a->horario, b->horario);
    // se horários diferentes, a decisão já está tomada
    if (compara_horario != 0) {
        return compara_horario;
    }
    //se empatou no horário, vai pelo nome da sala
    return strcmp(a->nome, b->nome);
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
