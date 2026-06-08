/*
Funcao no projeto: expor endpoint HTTP para buscas textuais, filtros e cadastro.
Conteudo: servidor HTTP simples, banco local CSV e integracao com algoritmos de busca.
O que faz: conecta o frontend ao backend em C com escolha automatica de algoritmo.
*/
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "buscas.h"
#include "ordenacao.h"
#include "dataset.h"
#include "util.h"
#include "busca_hash.h"
#include "arvore_avl.h"
#include "arvore_vp.h"


#define CAPACIDADE_DATASET 1024
#define REQ_BUF 16384
#define RESP_BUF 65536
#define DB_PATH "backend/data/locais.csv"

static void responder_agenda(int client_fd, const char *professor);
static void responder_busca_relevancia(int client_fd, const char *query);
static void responder_sugestao_avl(int client_fd, const char *query);
static void tratar_api_locais_delete(int client_fd, const char *uri);

typedef struct {
    FiltroLocal filtro;
    char nome[NOME_MAX];
    char responsavel[PROFESSOR_MAX];
    char bloco[BLOCO_MAX];
    char tipo[TIPO_MAX];
    char materia[MATERIA_MAX];
    char horario[HORARIO_MAX];
    int id_informado;
    int id_valor;
    int id_invalido;
} FiltroMutavel;

typedef struct {
    int ativa;
    OrdenacaoCampo campo;
    int crescente;
    OrdenacaoAlgoritmo algoritmo;
} ConfigOrdenacao;

static Local g_locais[CAPACIDADE_DATASET];
static size_t g_total_locais = 0;
static int g_banco_inicializado = 0;
static TabelaHash *g_tabela_hash = NULL;

typedef struct {
    int chave;
    size_t indice_original;
} ParChaveOrdenado;

static int parse_int(const char *texto, int *valor) {
    char *fim = NULL;
    long convertido = strtol(texto, &fim, 10);
    if (texto == fim || *fim != '\0') {
        return -1;
    }
    *valor = (int)convertido;
    return 0;
}

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

static void url_decode(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < cap; i++) {
        if (src[i] == '+') {
            dst[j++] = ' ';
            continue;
        }

        if (src[i] == '%' && src[i + 1] != '\0' && src[i + 2] != '\0') {
            int hi = hex_to_int(src[i + 1]);
            int lo = hex_to_int(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                dst[j++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }

        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static int query_get_param(const char *query, const char *chave, char *destino, size_t capacidade) {
    size_t tam_chave = strlen(chave);
    const char *cursor = query;

    while (cursor != NULL && *cursor != '\0') {
        const char *fim_param = strchr(cursor, '&');
        size_t tam_param = fim_param == NULL ? strlen(cursor) : (size_t)(fim_param - cursor);

        if (tam_param > tam_chave + 1 && strncmp(cursor, chave, tam_chave) == 0 && cursor[tam_chave] == '=') {
            size_t tam_valor = tam_param - tam_chave - 1;
            if (tam_valor >= capacidade) {
                tam_valor = capacidade - 1;
            }

            char bruto[1024];
            if (tam_valor >= sizeof(bruto)) {
                tam_valor = sizeof(bruto) - 1;
            }

            memcpy(bruto, cursor + tam_chave + 1, tam_valor);
            bruto[tam_valor] = '\0';
            url_decode(bruto, destino, capacidade);
            return 0;
        }

        if (fim_param == NULL) {
            break;
        }
        cursor = fim_param + 1;
    }

    return -1;
}

static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < cap; i++) {
        char c = src[i];
        if ((c == '"' || c == '\\') && j + 2 < cap) {
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n' && j + 2 < cap) {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

static int salvar_banco_csv(void) {
    FILE *fp = fopen(DB_PATH, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp, "id;nome;bloco;andar;tipo;capacidade;tem_ar;responsavel;materia;horario\n");
    for (size_t i = 0; i < g_total_locais; i++) {
        const Local *l = &g_locais[i];
        fprintf(
            fp,
            "%d;%s;%s;%d;%s;%d;%d;%s;%s;%s\n",
            l->id,
            l->nome,
            l->bloco,
            l->andar,
            l->tipo,
            l->capacidade,
            l->tem_ar,
            l->responsavel,
            l->materia,
            l->horario
        );
    }

    fclose(fp);
    return 0;
}

static size_t carregar_banco_csv(void) {
    FILE *fp = fopen(DB_PATH, "r");
    if (fp == NULL) {
        return 0;
    }

    char linha[2048];
    size_t total = 0;

    while (fgets(linha, sizeof(linha), fp) != NULL && total < CAPACIDADE_DATASET) {
        char *fim = strpbrk(linha, "\r\n");
        if (fim != NULL) {
            *fim = '\0';
        }

        if (linha[0] == '\0' || strncmp(linha, "id;", 3) == 0) {
            continue;
        }

        char *campos[10] = {0};
        int idx = 0;
        char *token = strtok(linha, ";");
        while (token != NULL && idx < 10) {
            campos[idx++] = token;
            token = strtok(NULL, ";");
        }

        if (idx != 9 && idx != 10) {
            continue;
        }

        Local local;
        memset(&local, 0, sizeof(local));

        if (parse_int(campos[0], &local.id) != 0 ||
            parse_int(campos[3], &local.andar) != 0 ||
            parse_int(campos[5], &local.capacidade) != 0 ||
            parse_int(campos[6], &local.tem_ar) != 0) {
            continue;
        }

        snprintf(local.nome, sizeof(local.nome), "%s", campos[1]);
        snprintf(local.bloco, sizeof(local.bloco), "%s", campos[2]);
        snprintf(local.tipo, sizeof(local.tipo), "%s", campos[4]);
        snprintf(local.responsavel, sizeof(local.responsavel), "%s", campos[7]);
        snprintf(local.materia, sizeof(local.materia), "%s", campos[8]);
        if (idx >= 10 && campos[9] != NULL) {
            snprintf(local.horario, sizeof(local.horario), "%s", campos[9]);
        } else {
            local.horario[0] = '\0';
        }

        g_locais[total++] = local;
    }

    fclose(fp);
    return total;
}

static int reconstruir_hash(void) {
    if (g_tabela_hash != NULL) {
        destruir_tabela_hash(g_tabela_hash);
        g_tabela_hash = NULL;
    }

    g_tabela_hash = criar_tabela_hash(200);
    if (g_tabela_hash == NULL) {
        return -1;
    }

    for (size_t i = 0; i < g_total_locais; i++) {
        inserir_hash(g_tabela_hash, &g_locais[i]);
    }

    return 0;
}

static void inicializar_banco(void) {
    if (g_banco_inicializado) {
        return;
    }

    g_total_locais = carregar_banco_csv();
    if (g_total_locais == 0) {
        g_total_locais = 0;
        (void)salvar_banco_csv();
    }

    ordenar_locais_por_id(g_locais, g_total_locais);
    g_banco_inicializado = 1;

    (void)reconstruir_hash();
}

//HASH
static int filtro_tem_algum_criterio_textual(const FiltroLocal *filtro) {
    return filtro->nome != NULL ||
           filtro->responsavel != NULL ||
           filtro->bloco != NULL ||
           filtro->tipo != NULL ||
           filtro->materia != NULL ||
           filtro->horario != NULL ||
           filtro->tem_ar != -1 ||
           filtro->andar_igual != -1 ||
           filtro->capacidade_minima != -1;
}

static int filtro_tem_algum_criterio_api(const FiltroMutavel *dados) {
    if (dados == NULL) {
        return 0;
    }

    return dados->id_informado || filtro_tem_algum_criterio_textual(&dados->filtro);
}

static void filtro_resetar(FiltroMutavel *destino) {
    memset(destino, 0, sizeof(*destino));
    destino->filtro.nome = NULL;
    destino->filtro.responsavel = NULL;
    destino->filtro.bloco = NULL;
    destino->filtro.tipo = NULL;
    destino->filtro.materia = NULL;
    destino->filtro.horario = NULL;
    destino->filtro.tem_ar = -1;
    destino->filtro.andar_igual = -1;
    destino->filtro.capacidade_minima = -1;
    destino->id_informado = 0;
    destino->id_valor = 0;
    destino->id_invalido = 0;
}

static void ordenacao_resetar(ConfigOrdenacao *destino) {
    if (destino == NULL) {
        return;
    }

    destino->ativa = 0;
    destino->campo = ORDENACAO_CAMPO_ID;
    destino->crescente = 1;
    destino->algoritmo = ORDENACAO_ALGORITMO_QUICKSORT;
}

static int preencher_ordenacao_por_query(
    ConfigOrdenacao *destino,
    const char *query,
    char *erro,
    size_t erro_cap
) {
    if (destino == NULL) {
        return -1;
    }

    if (query == NULL || *query == '\0') {
        return 0;
    }

    char ordenar_por[32] = {0};
    char algoritmo[32] = {0};
    char ordem[16] = {0};

    int tem_campo = query_get_param(query, "ordenarPor", ordenar_por, sizeof(ordenar_por)) == 0 &&
                    ordenar_por[0] != '\0';
    int tem_algoritmo = query_get_param(query, "algoritmoOrdenacao", algoritmo, sizeof(algoritmo)) == 0 &&
                        algoritmo[0] != '\0';
    int tem_ordem = query_get_param(query, "ordem", ordem, sizeof(ordem)) == 0 && ordem[0] != '\0';

    if (!tem_campo && !tem_algoritmo && !tem_ordem) {
        return 0;
    }

    destino->ativa = 1;

    if (tem_campo) {
        if (strings_iguais_case_insensitive(ordenar_por, "id")) {
            destino->campo = ORDENACAO_CAMPO_ID;
        } else if (strings_iguais_case_insensitive(ordenar_por, "nome")) {
            destino->campo = ORDENACAO_CAMPO_NOME;
        } else if (strings_iguais_case_insensitive(ordenar_por, "capacidade")) {
            destino->campo = ORDENACAO_CAMPO_CAPACIDADE;
        } else {
            snprintf(erro, erro_cap, "ordenarPor invalido. Use: id, nome ou capacidade");
            return -1;
        }
    }

    if (tem_algoritmo) {
        if (strings_iguais_case_insensitive(algoritmo, "quicksort")) {
            destino->algoritmo = ORDENACAO_ALGORITMO_QUICKSORT;
        } else if (strings_iguais_case_insensitive(algoritmo, "mergesort")) {
            destino->algoritmo = ORDENACAO_ALGORITMO_MERGESORT;
        } else if (strings_iguais_case_insensitive(algoritmo, "heapsort")) {
            destino->algoritmo = ORDENACAO_ALGORITMO_HEAPSORT;
        } else {
            snprintf(erro, erro_cap, "algoritmoOrdenacao invalido. Use: quicksort, mergesort ou heapsort");
            return -1;
        }
    }

    if (tem_ordem) {
        if (strings_iguais_case_insensitive(ordem, "asc")) {
            destino->crescente = 1;
        } else if (strings_iguais_case_insensitive(ordem, "desc")) {
            destino->crescente = 0;
        } else {
            snprintf(erro, erro_cap, "ordem invalida. Use: asc ou desc");
            return -1;
        }
    }

    return 0;
}

static void preencher_filtro_por_query(FiltroMutavel *destino, const char *query) {
    if (query == NULL) {
        return;
    }

    char int_texto[16] = {0};

    if (query_get_param(query, "nome", destino->nome, sizeof(destino->nome)) == 0 && destino->nome[0] != '\0') {
        destino->filtro.nome = destino->nome;
    }
    if (query_get_param(query, "responsavel", destino->responsavel, sizeof(destino->responsavel)) == 0 && destino->responsavel[0] != '\0') {
        destino->filtro.responsavel = destino->responsavel;
    } else if (query_get_param(query, "professor", destino->responsavel, sizeof(destino->responsavel)) == 0 && destino->responsavel[0] != '\0') {
        destino->filtro.responsavel = destino->responsavel;
    }
    if (query_get_param(query, "bloco", destino->bloco, sizeof(destino->bloco)) == 0 && destino->bloco[0] != '\0') {
        destino->filtro.bloco = destino->bloco;
    }
    if (query_get_param(query, "tipo", destino->tipo, sizeof(destino->tipo)) == 0 && destino->tipo[0] != '\0') {
        destino->filtro.tipo = destino->tipo;
    }
    if (query_get_param(query, "materia", destino->materia, sizeof(destino->materia)) == 0 && destino->materia[0] != '\0') {
        destino->filtro.materia = destino->materia;
    }
    if (query_get_param(query, "horario", destino->horario, sizeof(destino->horario)) == 0 && destino->horario[0] != '\0') {
        destino->filtro.horario = destino->horario;
    }

    if (query_get_param(query, "temAr", int_texto, sizeof(int_texto)) == 0 && int_texto[0] != '\0') {
        int valor = -1;
        if (parse_int(int_texto, &valor) == 0) {
            destino->filtro.tem_ar = valor;
        }
    }

    memset(int_texto, 0, sizeof(int_texto));
    if (query_get_param(query, "andar", int_texto, sizeof(int_texto)) == 0 && int_texto[0] != '\0') {
        int valor = -1;
        if (parse_int(int_texto, &valor) == 0) {
            destino->filtro.andar_igual = valor;
        }
    }

    memset(int_texto, 0, sizeof(int_texto));
    if (query_get_param(query, "capacidadeMin", int_texto, sizeof(int_texto)) == 0 && int_texto[0] != '\0') {
        int valor = -1;
        if (parse_int(int_texto, &valor) == 0) {
            destino->filtro.capacidade_minima = valor;
        }
    }

    memset(int_texto, 0, sizeof(int_texto));
    if (query_get_param(query, "capacidade", int_texto, sizeof(int_texto)) == 0 && int_texto[0] != '\0') {
        int valor = -1;
        if (parse_int(int_texto, &valor) == 0) {
            if (destino->filtro.capacidade_minima == -1 || valor > destino->filtro.capacidade_minima) {
                destino->filtro.capacidade_minima = valor;
            }
        }
    }

    memset(int_texto, 0, sizeof(int_texto));
    if (query_get_param(query, "id", int_texto, sizeof(int_texto)) == 0 && int_texto[0] != '\0') {
        int valor = 0;
        if (parse_int(int_texto, &valor) == 0) {
            destino->id_informado = 1;
            destino->id_valor = valor;
        } else {
            destino->id_invalido = 1;
        }
    }
}

static void enviar_resposta(
    int client_fd,
    int status,
    const char *status_text,
    const char *content_type,
    const char *body
) {
    char header[512];
    size_t body_len = strlen(body);

    int n = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status,
        status_text,
        content_type,
        body_len
    );

    if (n > 0) {
        (void)write(client_fd, header, (size_t)n);
    }
    (void)write(client_fd, body, body_len);
}

static void responder_json_erro(int client_fd, int status, const char *msg) {
    char body[512];
    snprintf(body, sizeof(body), "{\"ok\":false,\"erro\":\"%s\"}", msg);

    if (status == 404) {
        enviar_resposta(client_fd, 404, "Not Found", "application/json; charset=utf-8", body);
    } else if (status == 405) {
        enviar_resposta(client_fd, 405, "Method Not Allowed", "application/json; charset=utf-8", body);
    } else if (status == 500) {
        enviar_resposta(client_fd, 500, "Internal Server Error", "application/json; charset=utf-8", body);
    } else {
        enviar_resposta(client_fd, 400, "Bad Request", "application/json; charset=utf-8", body);
    }
}

static int json_append_local(char *body, size_t capacidade, size_t *cursor, const Local *local, int coma) {
    char nome[160], bloco[64], tipo[80], responsavel[160], materia[160], horario[80];
    json_escape(local->nome, nome, sizeof(nome));
    json_escape(local->bloco, bloco, sizeof(bloco));
    json_escape(local->tipo, tipo, sizeof(tipo));
    json_escape(local->responsavel, responsavel, sizeof(responsavel));
    json_escape(local->materia, materia, sizeof(materia));
    json_escape(local->horario, horario, sizeof(horario));

    int n = snprintf(
        body + *cursor,
        capacidade - *cursor,
        "%s{\"id\":%d,\"nome\":\"%s\",\"bloco\":\"%s\",\"andar\":%d,\"tipo\":\"%s\",\"capacidade\":%d,\"temAr\":%d,\"responsavel\":\"%s\",\"materia\":\"%s\",\"horario\":\"%s\"}",
        coma ? "," : "",
        local->id,
        nome,
        bloco,
        local->andar,
        tipo,
        local->capacidade,
        local->tem_ar,
        responsavel,
        materia,
        horario
    );

    if (n < 0 || (size_t)n >= capacidade - *cursor) {
        return -1;
    }

    *cursor += (size_t)n;
    return 0;
}

static int comparar_par_chave(const void *a, const void *b) {
    const ParChaveOrdenado *pa = (const ParChaveOrdenado *)a;
    const ParChaveOrdenado *pb = (const ParChaveOrdenado *)b;

    if (pa->chave < pb->chave) {
        return -1;
    }
    if (pa->chave > pb->chave) {
        return 1;
    }
    if (pa->indice_original < pb->indice_original) {
        return -1;
    }
    if (pa->indice_original > pb->indice_original) {
        return 1;
    }
    return 0;
}

static int hash_case_insensitive(const char *texto) {
    if (texto == NULL) {
        return 0;
    }

    unsigned long hash = 5381;
    for (size_t i = 0; texto[i] != '\0'; i++) {
        unsigned char c = (unsigned char)tolower((unsigned char)texto[i]);
        hash = ((hash << 5) + hash) + (unsigned long)c;
    }

    return (int)(hash & 0x7fffffffUL);
}

typedef enum {
    CAMPO_PRINCIPAL_NENHUM = 0,
    CAMPO_PRINCIPAL_ID,
    CAMPO_PRINCIPAL_NOME,
    CAMPO_PRINCIPAL_BLOCO,
    CAMPO_PRINCIPAL_TIPO,
    CAMPO_PRINCIPAL_PROFESSOR,
    CAMPO_PRINCIPAL_MATERIA,
    CAMPO_PRINCIPAL_HORARIO,
    CAMPO_PRINCIPAL_ANDAR,
    CAMPO_PRINCIPAL_CAPACIDADE,
    CAMPO_PRINCIPAL_TEM_AR
} CampoPrincipal;

static CampoPrincipal escolher_campo_principal(const FiltroMutavel *dados_filtro) {
    if (dados_filtro == NULL) {
        return CAMPO_PRINCIPAL_NENHUM;
    }

    if (dados_filtro->id_informado) {
        return CAMPO_PRINCIPAL_ID;
    }
    if (dados_filtro->filtro.bloco != NULL) {
        return CAMPO_PRINCIPAL_BLOCO;
    }
    if (dados_filtro->filtro.materia != NULL) {
        return CAMPO_PRINCIPAL_MATERIA;
    }
    if (dados_filtro->filtro.responsavel != NULL) {
        return CAMPO_PRINCIPAL_PROFESSOR;
    }
    if (dados_filtro->filtro.tipo != NULL) {
        return CAMPO_PRINCIPAL_TIPO;
    }
    if (dados_filtro->filtro.nome != NULL) {
        return CAMPO_PRINCIPAL_NOME;
    }
    if (dados_filtro->filtro.horario != NULL) {
        return CAMPO_PRINCIPAL_HORARIO;
    }
    if (dados_filtro->filtro.andar_igual != -1) {
        return CAMPO_PRINCIPAL_ANDAR;
    }
    if (dados_filtro->filtro.capacidade_minima != -1) {
        return CAMPO_PRINCIPAL_CAPACIDADE;
    }
    if (dados_filtro->filtro.tem_ar != -1) {
        return CAMPO_PRINCIPAL_TEM_AR;
    }

    return CAMPO_PRINCIPAL_NENHUM;
}

static const char *nome_campo_principal(CampoPrincipal campo) {
    switch (campo) {
        case CAMPO_PRINCIPAL_ID:
            return "id";
        case CAMPO_PRINCIPAL_NOME:
            return "nome";
        case CAMPO_PRINCIPAL_BLOCO:
            return "bloco";
        case CAMPO_PRINCIPAL_TIPO:
            return "tipo";
        case CAMPO_PRINCIPAL_PROFESSOR:
            return "responsavel";
        case CAMPO_PRINCIPAL_MATERIA:
            return "materia";
        case CAMPO_PRINCIPAL_HORARIO:
            return "horario";
        case CAMPO_PRINCIPAL_ANDAR:
            return "andar";
        case CAMPO_PRINCIPAL_CAPACIDADE:
            return "capacidade";
        case CAMPO_PRINCIPAL_TEM_AR:
            return "temAr";
        default:
            return "textual";
    }
}

static int chave_de_local_por_campo(const Local *local, CampoPrincipal campo) {
    if (local == NULL) {
        return 0;
    }

    switch (campo) {
        case CAMPO_PRINCIPAL_ID:
            return local->id;
        case CAMPO_PRINCIPAL_NOME:
            return hash_case_insensitive(local->nome);
        case CAMPO_PRINCIPAL_BLOCO:
            return hash_case_insensitive(local->bloco);
        case CAMPO_PRINCIPAL_TIPO:
            return hash_case_insensitive(local->tipo);
        case CAMPO_PRINCIPAL_PROFESSOR:
            return hash_case_insensitive(local->responsavel);
        case CAMPO_PRINCIPAL_MATERIA:
            return hash_case_insensitive(local->materia);
        case CAMPO_PRINCIPAL_HORARIO:
            return hash_case_insensitive(local->horario);
        case CAMPO_PRINCIPAL_ANDAR:
            return local->andar;
        case CAMPO_PRINCIPAL_CAPACIDADE:
            return local->capacidade;
        case CAMPO_PRINCIPAL_TEM_AR:
            return local->tem_ar;
        default:
            return 0;
    }
}

static int chave_alvo_por_campo(const FiltroMutavel *dados_filtro, CampoPrincipal campo) {
    if (dados_filtro == NULL) {
        return 0;
    }

    switch (campo) {
        case CAMPO_PRINCIPAL_ID:
            return dados_filtro->id_valor;
        case CAMPO_PRINCIPAL_NOME:
            return hash_case_insensitive(dados_filtro->filtro.nome);
        case CAMPO_PRINCIPAL_BLOCO:
            return hash_case_insensitive(dados_filtro->filtro.bloco);
        case CAMPO_PRINCIPAL_TIPO:
            return hash_case_insensitive(dados_filtro->filtro.tipo);
        case CAMPO_PRINCIPAL_PROFESSOR:
            return hash_case_insensitive(dados_filtro->filtro.responsavel);
        case CAMPO_PRINCIPAL_MATERIA:
            return hash_case_insensitive(dados_filtro->filtro.materia);
        case CAMPO_PRINCIPAL_HORARIO:
            return hash_case_insensitive(dados_filtro->filtro.horario);
        case CAMPO_PRINCIPAL_ANDAR:
            return dados_filtro->filtro.andar_igual;
        case CAMPO_PRINCIPAL_CAPACIDADE:
            return dados_filtro->filtro.capacidade_minima;
        case CAMPO_PRINCIPAL_TEM_AR:
            return dados_filtro->filtro.tem_ar;
        default:
            return 0;
    }
}

static int local_casa_campo_principal(const Local *local, const FiltroMutavel *dados_filtro, CampoPrincipal campo) {
    if (local == NULL || dados_filtro == NULL) {
        return 0;
    }

    switch (campo) {
        case CAMPO_PRINCIPAL_ID:
            return local->id == dados_filtro->id_valor;
        case CAMPO_PRINCIPAL_NOME:
            return dados_filtro->filtro.nome != NULL &&
                   strings_iguais_case_insensitive(local->nome, dados_filtro->filtro.nome);
        case CAMPO_PRINCIPAL_BLOCO:
            return dados_filtro->filtro.bloco != NULL &&
                   strings_iguais_case_insensitive(local->bloco, dados_filtro->filtro.bloco);
        case CAMPO_PRINCIPAL_TIPO:
            return dados_filtro->filtro.tipo != NULL &&
                   strings_iguais_case_insensitive(local->tipo, dados_filtro->filtro.tipo);
        case CAMPO_PRINCIPAL_PROFESSOR:
            return dados_filtro->filtro.responsavel != NULL &&
                   strings_iguais_case_insensitive(local->responsavel, dados_filtro->filtro.responsavel);
        case CAMPO_PRINCIPAL_MATERIA:
            return dados_filtro->filtro.materia != NULL &&
                   strings_iguais_case_insensitive(local->materia, dados_filtro->filtro.materia);
        case CAMPO_PRINCIPAL_HORARIO:
            return dados_filtro->filtro.horario != NULL &&
                   strings_iguais_case_insensitive(local->horario, dados_filtro->filtro.horario);
        case CAMPO_PRINCIPAL_ANDAR:
            return local->andar == dados_filtro->filtro.andar_igual;
        case CAMPO_PRINCIPAL_CAPACIDADE:
            return dados_filtro->filtro.capacidade_minima != -1 &&
                   local->capacidade >= dados_filtro->filtro.capacidade_minima;
        case CAMPO_PRINCIPAL_TEM_AR:
            return local->tem_ar == dados_filtro->filtro.tem_ar;
        default:
            return 0;
    }
}

static int local_aceita_filtro_local(const Local *local, const FiltroLocal *filtro) {
    if (local == NULL || filtro == NULL) {
        return 0;
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

    if (filtro->materia != NULL &&
        !strings_iguais_case_insensitive(local->materia, filtro->materia)) {
        return 0;
    }
    if (filtro->horario != NULL &&
        !strings_iguais_case_insensitive(local->horario, filtro->horario)) {
        return 0;
    }

    if (filtro->tem_ar != -1 && local->tem_ar != filtro->tem_ar) {
        return 0;
    }

    if (filtro->andar_igual != -1 && local->andar != filtro->andar_igual) {
        return 0;
    }

    if (filtro->capacidade_minima != -1 && local->capacidade < filtro->capacidade_minima) {
        return 0;
    }

    return 1;
}

static int filtrar_locais_direto(
    const Local *locais,
    size_t total,
    const FiltroLocal *filtro,
    ResultadoBusca *resultado
) {
    if (locais == NULL || filtro == NULL || resultado == NULL) {
        return -1;
    }

    resultado_inicializar(resultado);

    for (size_t i = 0; i < total; i++) {
        resultado->comparacoes++;
        if (local_aceita_filtro_local(&locais[i], filtro)) {
            if (resultado_adicionar(resultado, i) != 0) {
                return -1;
            }
        }
    }

    return resultado->quantidade > 0 ? 0 : -1;
}

static void responder_locais_por_resultado(
    int client_fd,
    const Local *locais_base,
    size_t total_base,
    const ResultadoBusca *resultado,
    const char *consulta,
    const char *metodo_usado,
    const ConfigOrdenacao *ordenacao
) {
    Local locais_saida[MAX_RESULTADOS_BUSCA];
    size_t total_saida = 0;

    for (size_t i = 0; i < resultado->quantidade && total_saida < MAX_RESULTADOS_BUSCA; i++) {
        size_t idx = resultado->indices[i];
        if (idx < total_base) {
            locais_saida[total_saida++] = locais_base[idx];
        }
    }

    unsigned long comparacoes_ordenacao = 0;
    const char *ordenacao_aplicada = "padrao";
    if (ordenacao != NULL && ordenacao->ativa && total_saida > 1) {
        if (ordenar_locais(
                locais_saida,
                total_saida,
                ordenacao->campo,
                ordenacao->crescente,
                ordenacao->algoritmo,
                &comparacoes_ordenacao
            ) != 0) {
            responder_json_erro(client_fd, 500, "Falha ao ordenar resultados");
            return;
        }
        ordenacao_aplicada = ordenacao_nome_algoritmo(ordenacao->algoritmo);
    }

    const char *campo_ordenacao = "padrao";
    const char *ordem_ordenacao = "asc";
    if (ordenacao != NULL && ordenacao->ativa) {
        campo_ordenacao = ordenacao_nome_campo(ordenacao->campo);
        ordem_ordenacao = ordenacao->crescente ? "asc" : "desc";
    }

    char body[RESP_BUF];
    size_t cursor = 0;
    int n = snprintf(
        body + cursor,
        sizeof(body) - cursor,
        "{\"ok\":true,\"consulta\":\"%s\",\"metodoUsado\":\"%s\",\"comparacoes\":%lu,\"comparacoesOrdenacao\":%lu,\"ordenacao\":{\"algoritmo\":\"%s\",\"campo\":\"%s\",\"ordem\":\"%s\"},\"total\":%zu,\"locais\":[",
        consulta,
        metodo_usado,
        resultado->comparacoes,
        comparacoes_ordenacao,
        ordenacao_aplicada,
        campo_ordenacao,
        ordem_ordenacao,
        total_saida
    );
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }
    cursor += (size_t)n;

    for (size_t i = 0; i < total_saida; i++) {
        if (json_append_local(body, sizeof(body), &cursor, &locais_saida[i], i > 0) != 0) {
            responder_json_erro(client_fd, 500, "Resposta excedeu limite");
            return;
        }
    }

    n = snprintf(body + cursor, sizeof(body) - cursor, "]}");
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }

    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", body);
}

static void responder_ranking_capacidade(int client_fd, const char *query) {
    int limite = 10;
    char top_texto[16] = {0};

    if (query_get_param(query, "top", top_texto, sizeof(top_texto)) == 0 && top_texto[0] != '\0') {
        if (parse_int(top_texto, &limite) != 0 || limite <= 0) {
            responder_json_erro(client_fd, 400, "Parametro top invalido. Use inteiro > 0");
            return;
        }
    }

    if (g_total_locais == 0) {
        enviar_resposta(
            client_fd,
            200,
            "OK",
            "application/json; charset=utf-8",
            "{\"ok\":true,\"metodoUsado\":\"heapsort(capacidade,desc)\",\"comparacoes\":0,\"total\":0,\"locais\":[]}"
        );
        return;
    }

    if ((size_t)limite > g_total_locais) {
        limite = (int)g_total_locais;
    }

    Local ordenados[CAPACIDADE_DATASET];
    memcpy(ordenados, g_locais, g_total_locais * sizeof(Local));

    unsigned long comparacoes = 0;
    if (ordenar_locais(
            ordenados,
            g_total_locais,
            ORDENACAO_CAMPO_CAPACIDADE,
            0,
            ORDENACAO_ALGORITMO_HEAPSORT,
            &comparacoes
        ) != 0) {
        responder_json_erro(client_fd, 500, "Falha ao ordenar ranking");
        return;
    }

    char body[RESP_BUF];
    size_t cursor = 0;
    int n = snprintf(
        body + cursor,
        sizeof(body) - cursor,
        "{\"ok\":true,\"metodoUsado\":\"heapsort(capacidade,desc)\",\"comparacoes\":%lu,\"total\":%d,\"locais\":[",
        comparacoes,
        limite
    );
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }
    cursor += (size_t)n;

    for (int i = 0; i < limite; i++) {
        if (json_append_local(body, sizeof(body), &cursor, &ordenados[i], i > 0) != 0) {
            responder_json_erro(client_fd, 500, "Resposta excedeu limite");
            return;
        }
    }

    n = snprintf(body + cursor, sizeof(body) - cursor, "]}");
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }

    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", body);
}

// Endpoint da funcionalidade AVL: recebe filtros da URL, chama a arvore e devolve JSON.
static void responder_sugestao_avl(int client_fd, const char *query) {
    // Reaproveita a mesma estrutura de filtros das buscas existentes no projeto.
    FiltroMutavel dados_filtro;
    filtro_resetar(&dados_filtro);
    preencher_filtro_por_query(&dados_filtro, query);

    // Valida ID caso ele venha por engano na query; mantem o padrao das outras rotas.
    if (dados_filtro.id_invalido) {
        responder_json_erro(client_fd, 400, "Parametro id invalido");
        return;
    }

    // A capacidade minima e obrigatoria, pois ela e a chave da busca lower bound na AVL.
    if (dados_filtro.filtro.capacidade_minima < 0) {
        responder_json_erro(client_fd, 400, "Informe capacidadeMin com inteiro >= 0");
        return;
    }

    // Executa a regra principal: montar a AVL filtrada e procurar a menor capacidade suficiente.
    ResultadoAvlCapacidade resultado;
    if (avl_sugerir_por_capacidade(
            g_locais,
            g_total_locais,
            &dados_filtro.filtro,
            &resultado
        ) != 0) {
        responder_json_erro(client_fd, 500, "Falha ao consultar sugestao com AVL");
        return;
    }

    // Inicia o JSON com identificacao do metodo e capacidade pedida pelo usuario.
    char body[RESP_BUF];
    size_t cursor = 0;
    int n = snprintf(
        body + cursor,
        sizeof(body) - cursor,
        "{\"ok\":true,\"consulta\":\"capacidadeMin\",\"metodoUsado\":\"arvore_avl_lower_bound(capacidade)\",\"capacidadeSolicitada\":%d,",
        dados_filtro.filtro.capacidade_minima
    );
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }
    cursor += (size_t)n;

    // Escreve a capacidade ideal encontrada; null indica que nenhuma sala atende.
    if (resultado.capacidade_encontrada >= 0) {
        n = snprintf(
            body + cursor,
            sizeof(body) - cursor,
            "\"capacidadeEncontrada\":%d,",
            resultado.capacidade_encontrada
        );
    } else {
        n = snprintf(body + cursor, sizeof(body) - cursor, "\"capacidadeEncontrada\":null,");
    }
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }
    cursor += (size_t)n;

    // Adiciona metricas da AVL para aparecerem na tela e ajudarem na explicacao.
    n = snprintf(
        body + cursor,
        sizeof(body) - cursor,
        "\"totalIndexados\":%zu,\"comparacoes\":%lu,\"rotacoes\":%lu,\"alturaArvore\":%d,\"total\":%zu,\"locais\":[",
        resultado.total_indexados,
        resultado.comparacoes,
        resultado.rotacoes,
        resultado.altura_arvore,
        resultado.quantidade
    );
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }
    cursor += (size_t)n;

    // Converte os indices retornados pela AVL em objetos Local completos no JSON.
    for (size_t i = 0; i < resultado.quantidade; i++) {
        size_t idx = resultado.indices[i];
        if (idx >= g_total_locais) {
            continue;
        }
        if (json_append_local(body, sizeof(body), &cursor, &g_locais[idx], i > 0) != 0) {
            responder_json_erro(client_fd, 500, "Resposta excedeu limite");
            return;
        }
    }

    // Fecha o array de locais e envia a resposta HTTP para o frontend.
    n = snprintf(body + cursor, sizeof(body) - cursor, "]}");
    if (n < 0 || (size_t)n >= sizeof(body) - cursor) {
        responder_json_erro(client_fd, 500, "Falha ao montar resposta");
        return;
    }

    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", body);
}

static void adicionar_candidatos_de_resultado(
    const Local *locais,
    size_t total,
    const Local *locais_ordenados_por_chave,
    size_t total_ordenado,
    const size_t *mapa_para_original,
    const ResultadoBusca *resultado_busca,
    CampoPrincipal campo,
    const FiltroMutavel *dados_filtro,
    unsigned char *ja_adicionado,
    Local *candidatos,
    size_t capacidade_candidatos,
    size_t *total_candidatos,
    unsigned long *comparacoes
) {
    if (locais == NULL || locais_ordenados_por_chave == NULL || mapa_para_original == NULL ||
        resultado_busca == NULL || dados_filtro == NULL || ja_adicionado == NULL ||
        candidatos == NULL || total_candidatos == NULL || comparacoes == NULL) {
        return;
    }

    if (resultado_busca->indice_unico < 0) {
        return;
    }

    size_t posicao = (size_t)resultado_busca->indice_unico;
    if (posicao >= total_ordenado) {
        return;
    }

    int chave_alvo = locais_ordenados_por_chave[posicao].id;
    size_t inicio = posicao;
    while (inicio > 0 && locais_ordenados_por_chave[inicio - 1].id == chave_alvo) {
        inicio--;
        (*comparacoes)++;
    }

    size_t fim = posicao;
    while (fim + 1 < total_ordenado && locais_ordenados_por_chave[fim + 1].id == chave_alvo) {
        fim++;
        (*comparacoes)++;
    }

    for (size_t i = inicio; i <= fim && *total_candidatos < capacidade_candidatos; i++) {
        size_t idx_original = mapa_para_original[i];
        if (idx_original >= total || idx_original >= CAPACIDADE_DATASET) {
            continue;
        }

        (*comparacoes)++;
        if (!local_casa_campo_principal(&locais[idx_original], dados_filtro, campo)) {
            continue;
        }

        if (ja_adicionado[idx_original]) {
            continue;
        }

        ja_adicionado[idx_original] = 1;
        candidatos[*total_candidatos] = locais[idx_original];
        (*total_candidatos)++;
    }
}

static size_t coletar_candidatos_tres_buscas(
    const Local *locais,
    size_t total,
    const FiltroMutavel *dados_filtro,
    CampoPrincipal campo,
    Local *candidatos,
    size_t capacidade_candidatos,
    unsigned long *comparacoes
) {
    if (locais == NULL || dados_filtro == NULL || candidatos == NULL || comparacoes == NULL ||
        total == 0 || capacidade_candidatos == 0 || campo == CAMPO_PRINCIPAL_NENHUM) {
        return 0;
    }

    if (total > CAPACIDADE_DATASET) {
        total = CAPACIDADE_DATASET;
    }

    ParChaveOrdenado pares[CAPACIDADE_DATASET];
    Local locais_ordenados_por_chave[CAPACIDADE_DATASET];
    size_t mapa_para_original[CAPACIDADE_DATASET];

    for (size_t i = 0; i < total; i++) {
        pares[i].chave = chave_de_local_por_campo(&locais[i], campo);
        pares[i].indice_original = i;
    }

    qsort(pares, total, sizeof(ParChaveOrdenado), comparar_par_chave);

    for (size_t i = 0; i < total; i++) {
        memset(&locais_ordenados_por_chave[i], 0, sizeof(Local));
        locais_ordenados_por_chave[i].id = pares[i].chave;
        mapa_para_original[i] = pares[i].indice_original;
    }

    int chave_alvo = chave_alvo_por_campo(dados_filtro, campo);

    ResultadoBusca resultado_binaria;
    ResultadoBusca resultado_indexada;
    ResultadoBusca resultado_interpolacao;

    int status_binaria = busca_binaria_por_id(locais_ordenados_por_chave, total, chave_alvo, &resultado_binaria);

    EntradaIndice indice[CAPACIDADE_DATASET];
    size_t passo = total / 8;
    if (passo == 0) {
        passo = 1;
    }
    size_t total_indice = construir_indice_primario(
        locais_ordenados_por_chave,
        total,
        passo,
        indice,
        CAPACIDADE_DATASET
    );

    int status_indexada = -1;
    if (total_indice > 0) {
        status_indexada = busca_sequencial_indexada_por_id(
            locais_ordenados_por_chave,
            total,
            chave_alvo,
            indice,
            total_indice,
            &resultado_indexada
        );
    } else {
        resultado_inicializar(&resultado_indexada);
    }

    int status_interpolacao = busca_interpolacao_por_id(
        locais_ordenados_por_chave,
        total,
        chave_alvo,
        &resultado_interpolacao
    );

    *comparacoes += resultado_binaria.comparacoes;
    *comparacoes += resultado_indexada.comparacoes;
    *comparacoes += resultado_interpolacao.comparacoes;

    unsigned char ja_adicionado[CAPACIDADE_DATASET];
    memset(ja_adicionado, 0, sizeof(ja_adicionado));
    size_t total_candidatos = 0;

    if (status_binaria == 0) {
        adicionar_candidatos_de_resultado(
            locais,
            total,
            locais_ordenados_por_chave,
            total,
            mapa_para_original,
            &resultado_binaria,
            campo,
            dados_filtro,
            ja_adicionado,
            candidatos,
            capacidade_candidatos,
            &total_candidatos,
            comparacoes
        );
    }
    if (status_indexada == 0) {
        adicionar_candidatos_de_resultado(
            locais,
            total,
            locais_ordenados_por_chave,
            total,
            mapa_para_original,
            &resultado_indexada,
            campo,
            dados_filtro,
            ja_adicionado,
            candidatos,
            capacidade_candidatos,
            &total_candidatos,
            comparacoes
        );
    }
    if (status_interpolacao == 0) {
        adicionar_candidatos_de_resultado(
            locais,
            total,
            locais_ordenados_por_chave,
            total,
            mapa_para_original,
            &resultado_interpolacao,
            campo,
            dados_filtro,
            ja_adicionado,
            candidatos,
            capacidade_candidatos,
            &total_candidatos,
            comparacoes
        );
    }

    if (campo == CAMPO_PRINCIPAL_CAPACIDADE && dados_filtro->filtro.capacidade_minima != -1) {
        int capacidade_minima = dados_filtro->filtro.capacidade_minima;

        size_t esquerda = 0;
        size_t direita = total;
        while (esquerda < direita) {
            size_t meio = esquerda + (direita - esquerda) / 2;
            (*comparacoes)++;
            if (locais_ordenados_por_chave[meio].id < capacidade_minima) {
                esquerda = meio + 1;
            } else {
                direita = meio;
            }
        }

        for (size_t i = esquerda; i < total && total_candidatos < capacidade_candidatos; i++) {
            size_t idx_original = mapa_para_original[i];
            if (idx_original >= total || idx_original >= CAPACIDADE_DATASET) {
                continue;
            }
            if (ja_adicionado[idx_original]) {
                continue;
            }
            ja_adicionado[idx_original] = 1;
            candidatos[total_candidatos++] = locais[idx_original];
        }
    }

    return total_candidatos;
}

static void responder_busca_automatica(
    int client_fd,
    const FiltroMutavel *dados_filtro,
    const ConfigOrdenacao *ordenacao
) {
    CampoPrincipal campo = escolher_campo_principal(dados_filtro);

    if (campo == CAMPO_PRINCIPAL_NOME && dados_filtro->filtro.nome != NULL && g_tabela_hash != NULL) {
        const Local *local_encontrado = buscar_hash(g_tabela_hash, dados_filtro->filtro.nome);

        ResultadoBusca resultado_hash;
        resultado_inicializar(&resultado_hash);

        if (local_encontrado != NULL) {
            size_t indice_original = (size_t)(local_encontrado - g_locais);
            if (indice_original < g_total_locais) {
                resultado_adicionar(&resultado_hash, indice_original);
            }
            resultado_hash.comparacoes = 1;
        }

        responder_locais_por_resultado(
            client_fd,
            g_locais,
            g_total_locais,
            &resultado_hash,
            "nome",
            "tabela_hash_O(1)",
            ordenacao
        );

        return;
    }

    if (campo == CAMPO_PRINCIPAL_NENHUM) {
        ResultadoBusca resultado;
        (void)filtrar_locais_direto(g_locais, g_total_locais, &dados_filtro->filtro, &resultado);
        responder_locais_por_resultado(
            client_fd,
            g_locais,
            g_total_locais,
            &resultado,
            "textual",
            "filtro_local",
            ordenacao
        );
        return;
    }

    Local candidatos[CAPACIDADE_DATASET];
    unsigned long comparacoes_prefiltro = 0;
    size_t total_candidatos = coletar_candidatos_tres_buscas(
        g_locais,
        g_total_locais,
        dados_filtro,
        campo,
        candidatos,
        CAPACIDADE_DATASET,
        &comparacoes_prefiltro
    );

    ResultadoBusca resultado_final;
    (void)filtrar_locais_direto(candidatos, total_candidatos, &dados_filtro->filtro, &resultado_final);
    resultado_final.comparacoes += comparacoes_prefiltro;

    char metodo_usado[160];
    const char *nome_campo = nome_campo_principal(campo);
    snprintf(
        metodo_usado,
        sizeof(metodo_usado),
        "binaria+indexada+interpolacao(%s)+filtro_local",
        nome_campo
    );

    responder_locais_por_resultado(
        client_fd,
        candidatos,
        total_candidatos,
        &resultado_final,
        nome_campo,
        metodo_usado,
        ordenacao
    );
}

static int id_ja_existe(int id) {
    for (size_t i = 0; i < g_total_locais; i++) {
        if (g_locais[i].id == id) {
            return 1;
        }
    }
    return 0;
}

static int parse_local_from_form(const char *body, Local *local, char *erro, size_t erro_cap) {
    char id_txt[32] = {0};
    char nome[NOME_MAX] = {0};
    char bloco[BLOCO_MAX] = {0};
    char andar_txt[32] = {0};
    char tipo[TIPO_MAX] = {0};
    char capacidade_txt[32] = {0};
    char tem_ar_txt[16] = {0};
    char responsavel[PROFESSOR_MAX] = {0};
    char materia[MATERIA_MAX] = {0};
    char horario[HORARIO_MAX] = {0};

    if (query_get_param(body, "id", id_txt, sizeof(id_txt)) != 0 ||
        query_get_param(body, "nome", nome, sizeof(nome)) != 0 ||
        query_get_param(body, "bloco", bloco, sizeof(bloco)) != 0 ||
        query_get_param(body, "andar", andar_txt, sizeof(andar_txt)) != 0 ||
        query_get_param(body, "tipo", tipo, sizeof(tipo)) != 0 ||
        query_get_param(body, "capacidade", capacidade_txt, sizeof(capacidade_txt)) != 0 ||
        query_get_param(body, "temAr", tem_ar_txt, sizeof(tem_ar_txt)) != 0 ||
        query_get_param(body, "materia", materia, sizeof(materia)) != 0 ||
        query_get_param(body, "horario", horario, sizeof(horario)) != 0) {
        snprintf(erro, erro_cap, "Campos obrigatorios ausentes no cadastro");
        return -1;
    }

    if (query_get_param(body, "responsavel", responsavel, sizeof(responsavel)) != 0 &&
        query_get_param(body, "professor", responsavel, sizeof(responsavel)) != 0) {
        snprintf(erro, erro_cap, "Campo obrigatorio ausente no cadastro: responsavel");
        return -1;
    }

    Local tmp;
    memset(&tmp, 0, sizeof(tmp));

    if (parse_int(id_txt, &tmp.id) != 0 ||
        parse_int(andar_txt, &tmp.andar) != 0 ||
        parse_int(capacidade_txt, &tmp.capacidade) != 0 ||
        parse_int(tem_ar_txt, &tmp.tem_ar) != 0) {
        snprintf(erro, erro_cap, "Campos numericos invalidos");
        return -1;
    }

    if (tmp.id <= 0 || tmp.capacidade < 0 || (tmp.tem_ar != 0 && tmp.tem_ar != 1)) {
        snprintf(erro, erro_cap, "Valores fora do intervalo permitido");
        return -1;
    }

    if (nome[0] == '\0' || bloco[0] == '\0' || tipo[0] == '\0' ||
        responsavel[0] == '\0' || materia[0] == '\0' || horario[0] == '\0') {
        snprintf(erro, erro_cap, "Campos textuais nao podem ser vazios");
        return -1;
    }

    if (id_ja_existe(tmp.id)) {
        snprintf(erro, erro_cap, "Ja existe local com esse id");
        return -1;
    }

    snprintf(tmp.nome, sizeof(tmp.nome), "%s", nome);
    snprintf(tmp.bloco, sizeof(tmp.bloco), "%s", bloco);
    snprintf(tmp.tipo, sizeof(tmp.tipo), "%s", tipo);
    snprintf(tmp.responsavel, sizeof(tmp.responsavel), "%s", responsavel);
    snprintf(tmp.materia, sizeof(tmp.materia), "%s", materia);
    snprintf(tmp.horario, sizeof(tmp.horario), "%s", horario);

    *local = tmp;
    return 0;
}

static void tratar_api_locais_post(int client_fd, const char *body) {
    if (g_total_locais >= CAPACIDADE_DATASET) {
        responder_json_erro(client_fd, 500, "Capacidade maxima atingida");
        return;
    }

    Local novo;
    // A MÁGICA: Zera completamente a memória da struct para matar os "fantasmas"
    memset(&novo, 0, sizeof(Local)); 
    
    char erro[256] = {0};
    if (parse_local_from_form(body, &novo, erro, sizeof(erro)) != 0) {
        responder_json_erro(client_fd, 400, erro);
        return;
    }

    // ÁRVORE VERMELHO-PRETA
    inicializar_arvore_vp();

    int novo_inicio, novo_fim;
    if (converter_horario_em_minutos(novo.horario, &novo_inicio, &novo_fim) == 0) {
        
        NoVP *raiz_sala = T_nil;

        for (size_t i = 0; i < g_total_locais; i++) {
            if (strings_iguais_case_insensitive(g_locais[i].nome, novo.nome)) {
                int ini_exist, fim_exist;
                if (converter_horario_em_minutos(g_locais[i].horario, &ini_exist, &fim_exist) == 0) {
                    NoVP *no = criar_no_vp(ini_exist, fim_exist, g_locais[i]);
                    inserir_arvore_vp(&raiz_sala, no);
                }
            }
        }

        NoVP *conflito = buscar_conflito_vp(raiz_sala, novo_inicio, novo_fim);

        if (conflito != NULL) {
            char msg_erro[512];
            snprintf(msg_erro, sizeof(msg_erro),
                "Conflito de horario! A sala '%s' ja esta reservada para a materia '%s' (%s).",
                novo.nome, conflito->local_dados.materia, conflito->local_dados.horario);

            responder_json_erro(client_fd, 409, msg_erro);
            
            // Limpa a RAM antes de abortar
            liberar_arvore_vp(raiz_sala); 
            return; 
        }
        
        // Se passou ileso (Aulas Coladas), limpa a RAM e segue para o cadastro
        liberar_arvore_vp(raiz_sala);
    }
    //fim arvore vp

    unsigned long movimentacoes = 0;
    if (inserir_local_ordenado_por_id(
            g_locais,
            &g_total_locais,
            CAPACIDADE_DATASET,
            &novo,
            &movimentacoes
        ) != 0) {
        responder_json_erro(client_fd, 500, "Falha ao inserir local em ordem");
        return;
    }

    if (reconstruir_hash() != 0) {
        responder_json_erro(client_fd, 500, "Falha ao atualizar indice hash");
        return;
    }

    if (salvar_banco_csv() != 0) {
        responder_json_erro(client_fd, 500, "Falha ao salvar no banco local");
        return;
    }

    char resposta[512];
    snprintf(
        resposta,
        sizeof(resposta),
        "{\"ok\":true,\"mensagem\":\"Local cadastrado com sucesso\",\"id\":%d,\"metodoInsercao\":\"insertion_sort_id\",\"movimentacoes\":%lu}",
        novo.id,
        movimentacoes
    );
    enviar_resposta(client_fd, 201, "Created", "application/json; charset=utf-8", resposta);
}

//QUICK SORT - RELEVANCIA-------------------------------------------------------------
static int json_append_relevancia(char *body, size_t capacidade, size_t *cursor, const LocalRelevancia *lr, int coma) {
    char temp_json[4096];
    size_t temp_cursor = 0;
    if (json_append_local(temp_json, sizeof(temp_json), &temp_cursor, lr->local, 0) != 0) return -1;
    temp_json[temp_cursor - 1] = '\0'; 
    int n = snprintf(body + *cursor, capacidade - *cursor, "%s%s,\"score\":%d}", coma ? "," : "", temp_json, lr->score);
    if (n < 0 || (size_t)n >= capacidade - *cursor) return -1;
    *cursor += (size_t)n;
    return 0;
}

static void responder_busca_relevancia(int client_fd, const char *query) {
    FiltroMutavel dados_filtro;
    filtro_resetar(&dados_filtro);
    preencher_filtro_por_query(&dados_filtro, query);

    if (!filtro_tem_algum_criterio_api(&dados_filtro)) {
        responder_json_erro(client_fd, 400, "Informe filtros para a relevancia.");
        return;
    }

    LocalRelevancia *vetor = (LocalRelevancia *)malloc(g_total_locais * sizeof(LocalRelevancia));
    if (!vetor) {
        responder_json_erro(client_fd, 500, "Erro de memoria.");
        return;
    }

    int qtd = 0;
    for (size_t i = 0; i < g_total_locais; i++) {
        int score = calcular_relevancia(&g_locais[i], &dados_filtro.filtro);
        if (score > 0) {
            vetor[qtd].local = &g_locais[i];
            vetor[qtd].score = score;
            qtd++;
        }
    }

    ordenar_por_relevancia(vetor, qtd);

    char *json = (char *)malloc(RESP_BUF);
    if (!json) {
        free(vetor);
        responder_json_erro(client_fd, 500, "Erro de memoria.");
        return;
    }

    size_t cursor = 0;
    cursor += snprintf(json + cursor, RESP_BUF - cursor, "{\"ok\":true,\"consulta\":\"relevancia\",\"metodoUsado\":\"Quick Sort (Match Score)\",\"total\":%d,\"locais\":[", qtd);

    for (int i = 0; i < qtd; i++) {
        json_append_relevancia(json, RESP_BUF, &cursor, &vetor[i], i > 0);
    }
    snprintf(json + cursor, RESP_BUF - cursor, "]}");

    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", json);
    free(json);
    free(vetor);
}
//FIM QUICK SORT --------------

static void tratar_conexao(int client_fd) {
    char req[REQ_BUF];
    ssize_t lidos = read(client_fd, req, sizeof(req) - 1);
    if (lidos <= 0) {
        return;
    }
    req[lidos] = '\0';

    char metodo_http[16];
    char uri[2048];
    if (sscanf(req, "%15s %2047s", metodo_http, uri) != 2) {
        responder_json_erro(client_fd, 400, "Requisicao invalida");
        return;
    }

    if (strcmp(metodo_http, "OPTIONS") == 0) {
        enviar_resposta(client_fd, 204, "No Content", "text/plain; charset=utf-8", "");
        return;
    }

    if (strcmp(uri, "/") == 0) {
        enviar_resposta(
            client_fd,
            200,
            "OK",
            "application/json; charset=utf-8",
            "{\"ok\":true,\"servico\":\"campus_api\",\"rotas\":[\"GET /api/busca\",\"GET /api/locais\",\"POST /api/locais\",\"GET /api/ranking/capacidade\",\"GET /api/sugestao/avl\"]}"
        );
        return;
    }

    if (strncmp(uri, "/api/ranking/capacidade", 23) == 0 &&
        (uri[23] == '\0' || uri[23] == '?')) {
        if (strcmp(metodo_http, "GET") != 0) {
            responder_json_erro(client_fd, 405, "Metodo HTTP nao suportado");
            return;
        }

        const char *query = strchr(uri, '?');
        responder_ranking_capacidade(client_fd, query == NULL ? "" : query + 1);
        return;
    }

    // Rota da AVL: sugere a menor sala que atende a capacidade minima solicitada.
    if (strncmp(uri, "/api/sugestao/avl", 17) == 0 &&
        (uri[17] == '\0' || uri[17] == '?')) {
        if (strcmp(metodo_http, "GET") != 0) {
            responder_json_erro(client_fd, 405, "Metodo HTTP nao suportado");
            return;
        }

        const char *query = strchr(uri, '?');
        responder_sugestao_avl(client_fd, query == NULL ? "" : query + 1);
        return;
    }


    // ==========================================
    // ROTA MERGE SORT
    // ==========================================
    if (strncmp(uri, "/api/agenda", 11) == 0) {
        if (strcmp(metodo_http, "GET") != 0) {
            responder_json_erro(client_fd, 405, "Metodo HTTP nao suportado");
            return;
        }
        const char *query = strchr(uri, '?');
        char professor[160] = {0};
        if (query != NULL) {
            query_get_param(query + 1, "responsavel", professor, sizeof(professor));
        }
        responder_agenda(client_fd, professor);
        return;
    }


    if (strncmp(uri, "/api/busca", 10) == 0) {
        if (strcmp(metodo_http, "GET") != 0) {
            responder_json_erro(client_fd, 405, "Metodo HTTP nao suportado");
            return;
        }

        const char *query = strchr(uri, '?');
        //QUICK RELEVANCIA
        char ordenar_por_check[32] = {0};
        if (query != NULL && query_get_param(query + 1, "ordenarPor", ordenar_por_check, sizeof(ordenar_por_check)) == 0) {
            if (strings_iguais_case_insensitive(ordenar_por_check, "relevancia")) {
                responder_busca_relevancia(client_fd, query == NULL ? "" : query + 1);
                return;
            }
        }

        FiltroMutavel dados_filtro;
        ConfigOrdenacao ordenacao;
        char erro_ordenacao[128] = {0};

        filtro_resetar(&dados_filtro);
        ordenacao_resetar(&ordenacao);
        preencher_filtro_por_query(&dados_filtro, query == NULL ? NULL : query + 1);
        if (preencher_ordenacao_por_query(&ordenacao, query == NULL ? NULL : query + 1, erro_ordenacao, sizeof(erro_ordenacao)) != 0) {
            responder_json_erro(client_fd, 400, erro_ordenacao);
            return;
        }

        if (dados_filtro.id_invalido) {
            responder_json_erro(client_fd, 400, "Parametro id invalido");
            return;
        }

        if (!filtro_tem_algum_criterio_api(&dados_filtro)) {
            responder_json_erro(client_fd, 400, "Informe ao menos um criterio (id, nome, materia, horario, bloco, etc)");
            return;
        }

        responder_busca_automatica(client_fd, &dados_filtro, &ordenacao);
        return;
    }

    if (strncmp(uri, "/api/locais", 11) == 0) {

        if (strcmp(metodo_http, "OPTIONS") == 0) {
            const char *res = "HTTP/1.1 204 No Content\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
            send(client_fd, res, strlen(res), 0);
            return; 
        }

        if (strcmp(metodo_http, "GET") == 0) {
            const char *query = strchr(uri, '?');
            FiltroMutavel dados_filtro;
            ConfigOrdenacao ordenacao;
            char erro_ordenacao[128] = {0};

            filtro_resetar(&dados_filtro);
            ordenacao_resetar(&ordenacao);
            preencher_filtro_por_query(&dados_filtro, query == NULL ? NULL : query + 1);
            if (preencher_ordenacao_por_query(&ordenacao, query == NULL ? NULL : query + 1, erro_ordenacao, sizeof(erro_ordenacao)) != 0) {
                responder_json_erro(client_fd, 400, erro_ordenacao);
                return;
            }
            if (dados_filtro.id_invalido) {
                responder_json_erro(client_fd, 400, "Parametro id invalido");
                return;
            }
            responder_busca_automatica(client_fd, &dados_filtro, &ordenacao);
            return;
        }

        if (strcmp(metodo_http, "POST") == 0) {
            const char *separador = strstr(req, "\r\n\r\n");
            if (separador == NULL) {
                responder_json_erro(client_fd, 400, "Corpo da requisicao ausente");
                return;
            }

            const char *body = separador + 4;
            tratar_api_locais_post(client_fd, body);
            return;
        }

        if (strcmp(metodo_http, "DELETE") == 0) {
            tratar_api_locais_delete(client_fd, uri);
            return;
        }

        responder_json_erro(client_fd, 405, "Metodo HTTP nao suportado");
        return;
    }

    responder_json_erro(client_fd, 404, "Rota nao encontrada");
}

// ==========================================
// MERGE SORT
// ==========================================
static void responder_agenda(int client_fd, const char *professor) {
    if (professor == NULL || professor[0] == '\0') {
        responder_json_erro(client_fd, 400, "Professor nao informado.");
        return;
    }

    Local *agenda = (Local *)malloc(g_total_locais * sizeof(Local));
    if (!agenda) {
        responder_json_erro(client_fd, 500, "Erro de memoria.");
        return;
    }

    int qtd = 0;
    for (size_t i = 0; i < g_total_locais; i++) {
        if (strings_iguais_case_insensitive(g_locais[i].responsavel, professor)) {
            agenda[qtd++] = g_locais[i];
        }
    }

    mergesort_agenda_iterativo(agenda, qtd);

    char *json = (char *)malloc(RESP_BUF);
    if (!json) {
        free(agenda);
        responder_json_erro(client_fd, 500, "Erro de memoria.");
        return;
    }

    size_t cursor = 0;
    cursor += snprintf(json + cursor, RESP_BUF - cursor, "{\"ok\":true,\"total\":%d,\"dados\":[", qtd);

    for (int i = 0; i < qtd; i++) {
        json_append_local(json, RESP_BUF, &cursor, &agenda[i], i > 0);
    }
    snprintf(json + cursor, RESP_BUF - cursor, "]}");

    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", json);
    free(json);
    free(agenda);
}

static void tratar_api_locais_delete(int client_fd, const char *uri) {
    char id_txt[32] = {0};
    const char *query = strchr(uri, '?');
    if (query != NULL) {
        query_get_param(query + 1, "id", id_txt, sizeof(id_txt));
    }

    if (id_txt[0] == '\0') {
        responder_json_erro(client_fd, 400, "ID do local ausente na URL");
        return;
    }

    int id_deletar = 0;
    if (parse_int(id_txt, &id_deletar) != 0 || id_deletar <= 0) {
        responder_json_erro(client_fd, 400, "ID invalido para exclusao");
        return;
    }
    int indice_localizado = -1;
    for (size_t i = 0; i < g_total_locais; i++) {
        if (g_locais[i].id == id_deletar) {
            indice_localizado = (int)i;
            break;
        }
    }

    if (indice_localizado == -1) {
        responder_json_erro(client_fd, 404, "Agendamento nao encontrado no banco");
        return;
    }

    Local local_alvo = g_locais[indice_localizado];
    inicializar_arvore_vp();
    NoVP *raiz_sala = T_nil;
    NoVP *no_para_deletar = T_nil;

    for (size_t i = 0; i < g_total_locais; i++) {
        if (strings_iguais_case_insensitive(g_locais[i].nome, local_alvo.nome)) {
            int ini, fim;
            if (converter_horario_em_minutos(g_locais[i].horario, &ini, &fim) == 0) {
                NoVP *no = criar_no_vp(ini, fim, g_locais[i]);
                inserir_arvore_vp(&raiz_sala, no);
                if (g_locais[i].id == id_deletar) {
                    no_para_deletar = no;
                }
            }
        }
    }

    if (no_para_deletar != T_nil) {
        remover_arvore_vp(&raiz_sala, no_para_deletar);
    }

    liberar_arvore_vp(raiz_sala);

    for (size_t i = indice_localizado; i < g_total_locais - 1; i++) {
        g_locais[i] = g_locais[i + 1];
    }
    g_total_locais--;

    reconstruir_hash();
    salvar_banco_csv();

    char resposta[256];
    snprintf(resposta, sizeof(resposta), 
        "{\"ok\":true,\"mensagem\":\"Agendamento excluido e reequilibrado via Arvore VP com sucesso!\"}");
    enviar_resposta(client_fd, 200, "OK", "application/json; charset=utf-8", resposta);
}

int main(int argc, char **argv) {
    int porta = 8091;
    if (argc >= 2 && parse_int(argv[1], &porta) != 0) {
        fprintf(stderr, "Porta invalida: %s\n", argv[1]);
        return 1;
    }

    inicializar_banco();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)porta);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("campus_api rodando em http://localhost:%d\n", porta);
    fflush(stdout);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        tratar_conexao(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 1;
}
