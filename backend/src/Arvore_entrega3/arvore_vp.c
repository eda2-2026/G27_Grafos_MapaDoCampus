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

//INSERÇÃO
static void conserta_insercao_vp(NoVP **raiz, NoVP *z) {
    while (z->pai->cor == VERMELHO) {
        
        // LADO ESQUERDO
        if (z->pai == z->pai->pai->esq) {
            NoVP *tio = z->pai->pai->dir;
            
            // CASO 3: O tio é VERMELHO
            if (tio->cor == VERMELHO) {
                z->pai->cor = PRETO;        // Pai fica preto
                tio->cor = PRETO;           // Tio fica preto
                z->pai->pai->cor = VERMELHO;// Avô fica vermelho
                z = z->pai->pai;            // Subimos o 'z' para o avô e o laço repete
            } 
            // CASO 4 e 5: O tio é PRETO
            else {
                // CASO 2: 'z' forma um "triângulo" (filho à direita)
                if (z == z->pai->dir) {
                    z = z->pai;
                    rotacao_esquerda(raiz, z); // Transforma num Caso 3 (linha reta)
                }
                
                // CASO 3: 'z' forma uma "linha reta" (filho à esquerda)
                z->pai->cor = PRETO;
                z->pai->pai->cor = VERMELHO;
                rotacao_direita(raiz, z->pai->pai);
            }
        } 
        // LADO DIREITO
        else {
            NoVP *tio = z->pai->pai->esq;
            
            if (tio->cor == VERMELHO) { //Caso 3 simétrico
                z->pai->cor = PRETO;
                tio->cor = PRETO;
                z->pai->pai->cor = VERMELHO;
                z = z->pai->pai;
            } else {
                if (z == z->pai->esq) { //Caso 4 simétrico
                    z = z->pai;
                    rotacao_direita(raiz, z);
                }
                //Caso 5 simétrico
                z->pai->cor = PRETO;
                z->pai->pai->cor = VERMELHO;
                rotacao_esquerda(raiz, z->pai->pai);
            }
        }
    }
    //RAIZ PRETA SEMPRE
    (*raiz)->cor = PRETO;
}

void inserir_arvore_vp(NoVP **raiz, NoVP *z) {
    NoVP *y = T_nil;   //y pai do novo nó
    NoVP *x = *raiz;   //x desce pela árvore

    //Descida Árvore Binária de Busca (O(log n))
    while (x != T_nil) {
        y = x;
        //comparacao
        if (z->inicio_minutos < x->inicio_minutos) {
            x = x->esq;
        } else {
            x = x->dir;
        }
    }

    z->pai = y;
    if (y == T_nil) {
        *raiz = z; //Se árvore vazia, ele é nova raiz
    } else if (z->inicio_minutos < y->inicio_minutos) {
        y->esq = z;
    } else {
        y->dir = z;
    }

    z->esq = T_nil;
    z->dir = T_nil;
    z->cor = VERMELHO;

    conserta_insercao_vp(raiz, z);
}

NoVP* buscar_conflito_vp(NoVP *raiz, int novo_inicio, int novo_fim) {
    NoVP *atual = raiz;

    while (atual != T_nil) {
        if (novo_inicio < atual->fim_minutos && novo_fim > atual->inicio_minutos) {
            return atual; 
        }

        if (novo_inicio < atual->inicio_minutos) {
            atual = atual->esq;
        } else {
            atual = atual->dir;
        }
    }

    return NULL; 
}

static NoVP* minimo_vp(NoVP *x) {
    while (x->esq != T_nil) x = x->esq;
    return x;
}

static void transplante_vp(NoVP **raiz, NoVP *u, NoVP *v) {
    if (u->pai == T_nil) *raiz = v;
    else if (u == u->pai->esq) u->pai->esq = v;
    else u->pai->dir = v;
    v->pai = u->pai; 
}

static void conserta_remocao_vp(NoVP **raiz, NoVP *x) {
    while (x != *raiz && x->cor == PRETO) {
        if (x == x->pai->esq) {
            NoVP *irmao = x->pai->dir;
            if (irmao->cor == VERMELHO) {
                irmao->cor = PRETO;
                x->pai->cor = VERMELHO;
                rotacao_esquerda(raiz, x->pai);
                irmao = x->pai->dir;
            }
            if (irmao->esq->cor == PRETO && irmao->dir->cor == PRETO) {
                irmao->cor = VERMELHO;
                x = x->pai;
            } else {
                if (irmao->dir->cor == PRETO) {
                    irmao->esq->cor = PRETO;
                    irmao->cor = VERMELHO;
                    rotacao_direita(raiz, irmao);
                    irmao = x->pai->dir;
                }
                irmao->cor = x->pai->cor;
                x->pai->cor = PRETO;
                irmao->dir->cor = PRETO;
                rotacao_esquerda(raiz, x->pai);
                x = *raiz;
            }
        } else {
            NoVP *irmao = x->pai->esq;
            if (irmao->cor == VERMELHO) {
                irmao->cor = PRETO;
                x->pai->cor = VERMELHO;
                rotacao_direita(raiz, x->pai);
                irmao = x->pai->esq;
            }
            if (irmao->dir->cor == PRETO && irmao->esq->cor == PRETO) {
                irmao->cor = VERMELHO;
                x = x->pai;
            } else {
                if (irmao->esq->cor == PRETO) {
                    irmao->dir->cor = PRETO;
                    irmao->cor = VERMELHO;
                    rotacao_esquerda(raiz, irmao);
                    irmao = x->pai->esq;
                }
                irmao->cor = x->pai->cor;
                x->pai->cor = PRETO;
                irmao->esq->cor = PRETO;
                rotacao_direita(raiz, x->pai);
                x = *raiz;
            }
        }
    }
    x->cor = PRETO;
}

void remover_arvore_vp(NoVP **raiz, NoVP *z) {
    if (z == NULL || z == T_nil) return;

    NoVP *y = z;
    NoVP *x;
    CorVP cor_original_y = y->cor;

    if (z->esq == T_nil) {
        x = z->dir;
        transplante_vp(raiz, z, z->dir);
    } else if (z->dir == T_nil) {
        x = z->esq;
        transplante_vp(raiz, z, z->esq);
    } else {
        y = minimo_vp(z->dir);
        cor_original_y = y->cor;
        x = y->dir;
        if (y->pai == z) {
            x->pai = y;
        } else {
            transplante_vp(raiz, y, y->dir);
            y->dir = z->dir;
            y->dir->pai = y;
        }
        transplante_vp(raiz, z, y);
        y->esq = z->esq;
        y->esq->pai = y;
        y->cor = z->cor;
    }

    if (cor_original_y == PRETO) {
        conserta_remocao_vp(raiz, x);
    }
    free(z);
}

NoVP* buscar_no_exato_vp(NoVP *raiz, int inicio) {
    NoVP *atual = raiz;
    while (atual != T_nil && inicio != atual->inicio_minutos) {
        if (inicio < atual->inicio_minutos) atual = atual->esq;
        else atual = atual->dir;
    }
    return atual;
}