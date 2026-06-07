#ifndef ARVORE_VP_H
#define ARVORE_VP_H

#include "local.h"

typedef enum {
    VERMELHO = 0,
    PRETO
} CorVP;

typedef struct NoVP {
    int inicio_minutos;
    int fim_minutos;
    CorVP cor;
    
    struct NoVP *pai;
    struct NoVP *esq;
    struct NoVP *dir;
    
    Local local_dados;
} NoVP;

extern NoVP *T_nil;

void inicializar_arvore_vp(void);
NoVP* criar_no_vp(int inicio, int fim, Local local);

//rotações
void rotacao_esquerda(NoVP **raiz, NoVP *x);
void rotacao_direita(NoVP **raiz, NoVP *y);

void inserir_arvore_vp(NoVP **raiz, NoVP *novo_no);

NoVP* buscar_conflito_vp(NoVP *raiz, int novo_inicio, int novo_fim);

NoVP* buscar_no_exato_vp(NoVP *raiz, int inicio);
void remover_arvore_vp(NoVP **raiz, NoVP *z);

void liberar_arvore_vp(NoVP *raiz);

#endif