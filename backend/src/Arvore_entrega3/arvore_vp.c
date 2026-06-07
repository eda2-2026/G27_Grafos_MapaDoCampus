#include "arvore_vp.h"
#include <stdlib.h>

NoVP *T_nil = NULL;

void inicializar_arvore_vp(void) {
    if (T_nil == NULL) {
        T_nil = (NoVP *)malloc(sizeof(NoVP));
        if (T_nil != NULL) {
            T_nil->inicio_minutos = -1;
            T_nil->fim_minutos = -1;
            T_nil->cor = PRETO;
            T_nil->pai = T_nil;
            T_nil->esq = T_nil;
            T_nil->dir = T_nil;
        }
    }
}

NoVP* criar_no_vp(int inicio, int fim, Local local) {
    NoVP *novo = (NoVP *)malloc(sizeof(NoVP));
    if (novo == NULL) {
        return NULL;
    }

    novo->inicio_minutos = inicio;
    novo->fim_minutos = fim;
    novo->cor = VERMELHO;
    
    //Todo nó novo nasce apontando para o sentinela
    novo->pai = T_nil;
    novo->esq = T_nil;
    novo->dir = T_nil;
    
    novo->local_dados = local;

    return novo;
}

//ROTAÇÕES
void rotacao_esquerda(NoVP **raiz, NoVP *x) {
    NoVP *y = x->dir;
    x->dir = y->esq;

    if (y->esq != T_nil) {
        y->esq->pai = x;
    }

    y->pai = x->pai;

    if (x->pai == T_nil) {
        *raiz = y;
    } 
    else if (x == x->pai->esq) {
        x->pai->esq = y;
    } 
    else {
        x->pai->dir = y;
    }

    y->esq = x;
    x->pai = y;
}

void rotacao_direita(NoVP **raiz, NoVP *y) {
    NoVP *x = y->esq;
    y->esq = x->dir;

    if (x->dir != T_nil) {
        x->dir->pai = y;
    }

    x->pai = y->pai;

    if (y->pai == T_nil) {
        *raiz = x;
    } 
    else if (y == y->pai->dir) {
        y->pai->dir = x;
    } 
    else {
        y->pai->esq = x;
    }

    x->dir = y;
    y->pai = x;
}

