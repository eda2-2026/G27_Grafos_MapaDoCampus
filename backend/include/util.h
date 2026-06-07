#ifndef UTIL_H
#define UTIL_H

#include "buscas.h"
#include "local.h"

int strings_iguais_case_insensitive(const char *a, const char *b);
int converter_horario_em_minutos(const char *horario_str, int *inicio_min, int *fim_min);
void imprimir_local(const Local *local);
void imprimir_resultado(const ResultadoBusca *resultado, const Local *locais);

#endif
