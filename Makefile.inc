# Para que no confunda a los comandos con nombres de archivos
.PHONY = clean, all
# Compilador, tiene un default del host (cc) pero se puede especificar cuando corremos make
# make all CC=/usr/bin/gcc
# Poner CC=clang scan-build -disable-checker deadcode.DeadStores -o /tmp/out make
COMPILER = ${CC}
# Flags para hacer que el compilador sea mas estricto
# Hay mas informacion sobre lo que hacen en https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wformat_003d2
CFLAGS = --std=c11 -Wall -pedantic -pedantic-errors -Wformat=2 -Wextra -Wno-unused-parameter -Wundef -Wuninitialized -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L -fsanitize=address -g -O3
# 200112L antes
# 200809L para usar fstatat
# Los de la clase
SERVER_DIR = ./server
SERVER_NAME = popserver
ADMIN_DIR = ./admin
ADMIN_NAME = popadmin
TARGET_DIR = ./bin
LOG_DIR = ./log
