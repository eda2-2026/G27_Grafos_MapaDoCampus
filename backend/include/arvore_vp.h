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

#endif