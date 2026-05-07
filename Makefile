# Arquivo: Makefile
# Funcao no projeto: centralizar comandos de build, teste e limpeza.
# Conteudo: variaveis de compilacao e alvos (build, run, run-api, check, clean).
# O que faz: padroniza o fluxo para toda a equipe com um conjunto unico de comandos.
CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -Ibackend/include -g
LDFLAGS :=
API_PORT := 8091

TARGET := bin/campus_busca
TARGET_API := bin/campus_api

SRC_COMMON := \
	backend/src/dataset.c \
	backend/src/util.c \
	backend/src/ordenacao.c \
	backend/src/busca_binaria.c \
	backend/src/busca_indexada.c \
	backend/src/busca_hash.c \
	backend/src/busca_interpolacao.c

MAIN_SRC := backend/src/main.c
API_SRC := backend/src/api_adapter.c

.PHONY: all build run run-api check clean

all: build

build: $(TARGET) $(TARGET_API)

$(TARGET): $(SRC_COMMON) $(MAIN_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_API): $(SRC_COMMON) $(API_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

run-api: $(TARGET_API)
	./$(TARGET_API) $(API_PORT)

check: build

clean:
	rm -rf bin
