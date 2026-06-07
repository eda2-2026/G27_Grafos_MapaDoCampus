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