#include "arvore_avl.h"

#include <stdlib.h>

#include "util.h"

// Lista encadeada usada para guardar varias salas com a mesma capacidade no mesmo no da AVL.
typedef struct ListaIndiceAvl {
    size_t indice;
    struct ListaIndiceAvl *prox;
} ListaIndiceAvl;

// No da AVL: a chave e a capacidade, e a altura permite calcular o balanceamento.
typedef struct NoAvlCapacidade {
    int capacidade;
    int altura;
    ListaIndiceAvl *indices;
    struct NoAvlCapacidade *esquerda;
    struct NoAvlCapacidade *direita;
} NoAvlCapacidade;

// Prepara o objeto de saida, evitando lixo de memoria nas metricas e nos totais.
void resultado_avl_inicializar(ResultadoAvlCapacidade *resultado) {
    if (resultado == NULL) {
        return;
    }

    resultado->quantidade = 0;
    resultado->total_indexados = 0;
    resultado->comparacoes = 0;
    resultado->rotacoes = 0;
    resultado->altura_arvore = 0;
    resultado->capacidade_encontrada = -1;
}

// Retorna o maior valor entre dois inteiros, usado no calculo da altura do no.
static int maior(int a, int b) {
    return a > b ? a : b;
}

// Retorna altura 0 para no nulo, simplificando os calculos recursivos da AVL.
static int altura_no(const NoAvlCapacidade *no) {
    return no == NULL ? 0 : no->altura;
}

// Calcula a diferenca entre altura da esquerda e da direita para saber se precisa rotacionar.
static int fator_balanceamento(const NoAvlCapacidade *no) {
    if (no == NULL) {
        return 0;
    }

    return altura_no(no->esquerda) - altura_no(no->direita);
}

// Recalcula a altura do no depois de uma insercao ou rotacao.
static void atualizar_altura(NoAvlCapacidade *no) {
    if (no != NULL) {
        no->altura = 1 + maior(altura_no(no->esquerda), altura_no(no->direita));
    }
}

// Cria um item da lista de indices, que aponta para uma sala no vetor original g_locais.
static ListaIndiceAvl *criar_item_indice(size_t indice) {
    ListaIndiceAvl *item = (ListaIndiceAvl *)malloc(sizeof(ListaIndiceAvl));
    if (item == NULL) {
        return NULL;
    }

    item->indice = indice;
    item->prox = NULL;
    return item;
}

// Cria um novo no AVL para uma capacidade ainda inexistente na arvore.
static NoAvlCapacidade *criar_no(int capacidade, size_t indice) {
    NoAvlCapacidade *no = (NoAvlCapacidade *)malloc(sizeof(NoAvlCapacidade));
    if (no == NULL) {
        return NULL;
    }

    no->indices = criar_item_indice(indice);
    if (no->indices == NULL) {
        free(no);
        return NULL;
    }

    no->capacidade = capacidade;
    no->altura = 1;
    no->esquerda = NULL;
    no->direita = NULL;
    return no;
}

// Adiciona outra sala ao no quando a capacidade ja existe na arvore.
static int adicionar_indice_no(NoAvlCapacidade *no, size_t indice) {
    ListaIndiceAvl *item = criar_item_indice(indice);
    if (item == NULL) {
        return -1;
    }

    // Capacidades iguais ficam no mesmo no, formando uma lista simples de salas.
    item->prox = no->indices;
    no->indices = item;
    return 0;
}

// Corrige desbalanceamento esquerda-esquerda, subindo o filho esquerdo.
static NoAvlCapacidade *rotacao_direita(NoAvlCapacidade *raiz, unsigned long *rotacoes) {
    NoAvlCapacidade *nova_raiz = raiz->esquerda;
    NoAvlCapacidade *subarvore_transferida = nova_raiz->direita;

    // Rotacao direita: o filho esquerdo sobe e a raiz antiga vira filho direito.
    nova_raiz->direita = raiz;
    raiz->esquerda = subarvore_transferida;

    atualizar_altura(raiz);
    atualizar_altura(nova_raiz);

    if (rotacoes != NULL) {
        (*rotacoes)++;
    }

    return nova_raiz;
}

// Corrige desbalanceamento direita-direita, subindo o filho direito.
static NoAvlCapacidade *rotacao_esquerda(NoAvlCapacidade *raiz, unsigned long *rotacoes) {
    NoAvlCapacidade *nova_raiz = raiz->direita;
    NoAvlCapacidade *subarvore_transferida = nova_raiz->esquerda;

    // Rotacao esquerda: o filho direito sobe e a raiz antiga vira filho esquerdo.
    nova_raiz->esquerda = raiz;
    raiz->direita = subarvore_transferida;

    atualizar_altura(raiz);
    atualizar_altura(nova_raiz);

    if (rotacoes != NULL) {
        (*rotacoes)++;
    }

    return nova_raiz;
}

// Insere uma sala na AVL usando a capacidade como chave e rebalanceia no retorno da recursao.
static NoAvlCapacidade *inserir_no_avl(
    NoAvlCapacidade *raiz,
    int capacidade,
    size_t indice,
    unsigned long *comparacoes,
    unsigned long *rotacoes,
    int *erro
) {
    // Caso base: lugar vazio encontrado, entao nasce um novo no na arvore.
    if (raiz == NULL) {
        NoAvlCapacidade *novo = criar_no(capacidade, indice);
        if (novo == NULL) {
            *erro = -1;
        }
        return novo;
    }

    (*comparacoes)++;
    // Decide o caminho da insercao como em uma arvore binaria de busca.
    if (capacidade < raiz->capacidade) {
        raiz->esquerda = inserir_no_avl(raiz->esquerda, capacidade, indice, comparacoes, rotacoes, erro);
    } else if (capacidade > raiz->capacidade) {
        raiz->direita = inserir_no_avl(raiz->direita, capacidade, indice, comparacoes, rotacoes, erro);
    } else {
        // Se a capacidade ja existe, a sala entra na lista desse mesmo no.
        if (adicionar_indice_no(raiz, indice) != 0) {
            *erro = -1;
        }
        return raiz;
    }

    if (*erro != 0) {
        return raiz;
    }

    atualizar_altura(raiz);

    // A AVL corrige qualquer diferenca de altura maior que 1 usando rotacoes.
    int balanceamento = fator_balanceamento(raiz);

    // Caso esquerda-esquerda: uma rotacao simples para a direita resolve.
    if (balanceamento > 1 && capacidade < raiz->esquerda->capacidade) {
        return rotacao_direita(raiz, rotacoes);
    }

    // Caso direita-direita: uma rotacao simples para a esquerda resolve.
    if (balanceamento < -1 && capacidade > raiz->direita->capacidade) {
        return rotacao_esquerda(raiz, rotacoes);
    }

    // Caso esquerda-direita: primeiro gira o filho para a esquerda, depois a raiz para a direita.
    if (balanceamento > 1 && capacidade > raiz->esquerda->capacidade) {
        raiz->esquerda = rotacao_esquerda(raiz->esquerda, rotacoes);
        return rotacao_direita(raiz, rotacoes);
    }

    // Caso direita-esquerda: primeiro gira o filho para a direita, depois a raiz para a esquerda.
    if (balanceamento < -1 && capacidade < raiz->direita->capacidade) {
        raiz->direita = rotacao_direita(raiz->direita, rotacoes);
        return rotacao_esquerda(raiz, rotacoes);
    }

    return raiz;
}

// Aplica os filtros opcionais antes de inserir a sala na AVL; capacidadeMin fica para a busca.
static int local_passa_filtros_sem_capacidade(const Local *local, const FiltroLocal *filtro) {
    if (local == NULL) {
        return 0;
    }

    if (filtro == NULL) {
        return 1;
    }

    if (filtro->nome != NULL && !strings_iguais_case_insensitive(local->nome, filtro->nome)) {
        return 0;
    }

    if (filtro->responsavel != NULL && !strings_iguais_case_insensitive(local->responsavel, filtro->responsavel)) {
        return 0;
    }

    if (filtro->bloco != NULL && !strings_iguais_case_insensitive(local->bloco, filtro->bloco)) {
        return 0;
    }

    if (filtro->tipo != NULL && !strings_iguais_case_insensitive(local->tipo, filtro->tipo)) {
        return 0;
    }

    if (filtro->materia != NULL && !strings_iguais_case_insensitive(local->materia, filtro->materia)) {
        return 0;
    }

    if (filtro->horario != NULL && !strings_iguais_case_insensitive(local->horario, filtro->horario)) {
        return 0;
    }

    if (filtro->tem_ar != -1 && local->tem_ar != filtro->tem_ar) {
        return 0;
    }

    if (filtro->andar_igual != -1 && local->andar != filtro->andar_igual) {
        return 0;
    }

    return 1;
}

// Busca lower bound: menor capacidade maior ou igual a capacidade_minima.
static NoAvlCapacidade *buscar_menor_capacidade_suficiente(
    NoAvlCapacidade *raiz,
    int capacidade_minima,
    unsigned long *comparacoes
) {
    NoAvlCapacidade *atual = raiz;
    NoAvlCapacidade *melhor = NULL;

    while (atual != NULL) {
        (*comparacoes)++;

        // Quando o no atual atende, ele vira candidato e tentamos achar algo menor na esquerda.
        if (atual->capacidade >= capacidade_minima) {
            melhor = atual;
            atual = atual->esquerda;
        } else {
            // Quando nao atende, so a subarvore da direita pode ter capacidade suficiente.
            atual = atual->direita;
        }
    }

    return melhor;
}

// Copia para o resultado todas as salas que estao na capacidade ideal encontrada.
static void copiar_indices_do_no(const NoAvlCapacidade *no, ResultadoAvlCapacidade *resultado) {
    const ListaIndiceAvl *item = no->indices;

    while (item != NULL && resultado->quantidade < AVL_MAX_SUGESTOES) {
        resultado->indices[resultado->quantidade] = item->indice;
        resultado->quantidade++;
        item = item->prox;
    }
}

// Libera a lista de salas armazenada dentro de um no da AVL.
static void destruir_lista_indices(ListaIndiceAvl *lista) {
    while (lista != NULL) {
        ListaIndiceAvl *proximo = lista->prox;
        free(lista);
        lista = proximo;
    }
}

// Libera a arvore inteira em pos-ordem para evitar vazamento de memoria.
static void destruir_arvore(NoAvlCapacidade *raiz) {
    if (raiz == NULL) {
        return;
    }

    destruir_arvore(raiz->esquerda);
    destruir_arvore(raiz->direita);
    destruir_lista_indices(raiz->indices);
    free(raiz);
}

// Funcao principal do modulo: monta a AVL filtrada e retorna a melhor sala por capacidade.
int avl_sugerir_por_capacidade(
    const Local *locais,
    size_t total,
    const FiltroLocal *filtro,
    ResultadoAvlCapacidade *resultado
) {
    if (locais == NULL || filtro == NULL || resultado == NULL || filtro->capacidade_minima < 0) {
        return -1;
    }

    resultado_avl_inicializar(resultado);

    // A raiz comeca vazia; cada sala valida sera inserida mantendo a AVL balanceada.
    NoAvlCapacidade *raiz = NULL;
    int erro = 0;

    for (size_t i = 0; i < total; i++) {
        // Filtros como bloco, horario e ar-condicionado reduzem o conjunto antes da AVL.
        if (!local_passa_filtros_sem_capacidade(&locais[i], filtro)) {
            continue;
        }

        // Insere a capacidade da sala na AVL e guarda o indice para recuperar o Local depois.
        raiz = inserir_no_avl(
            raiz,
            locais[i].capacidade,
            i,
            &resultado->comparacoes,
            &resultado->rotacoes,
            &erro
        );

        if (erro != 0) {
            destruir_arvore(raiz);
            return -1;
        }

        resultado->total_indexados++;
    }

    // Guarda a altura final para mostrar que a arvore permaneceu balanceada.
    resultado->altura_arvore = altura_no(raiz);

    // Busca a menor capacidade suficiente para a turma informada pelo usuario.
    NoAvlCapacidade *melhor = buscar_menor_capacidade_suficiente(
        raiz,
        filtro->capacidade_minima,
        &resultado->comparacoes
    );

    if (melhor != NULL) {
        // Quando encontrou uma capacidade ideal, retorna todas as salas empatadas nessa capacidade.
        resultado->capacidade_encontrada = melhor->capacidade;
        copiar_indices_do_no(melhor, resultado);
    }

    // A arvore e temporaria: depois da consulta, a memoria e liberada.
    destruir_arvore(raiz);
    return 0;
}
