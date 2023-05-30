# Para que no confunda a los comandos con nombres de archivos
.PHONY = clean, all
# Compilador, tiene un default del host (cc) pero se puede especificar cuando corremos make
# make all CC=/usr/bin/gcc
COMPILER = ${CC}
# Flags para hacer que el compilador sea mas estricto
# Hay mas informacion sobre lo que hacen en https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wformat_003d2
CFLAGS = --std=c11 -Wall -pedantic -fsanitize=address -Wformat=2 -Wextra -Wno-unused-parameter -Wundef -Wuninitialized -Wimplicit-fallthrough -g

SERVER_DIR = ./server
SERVER_NAME = popserver
ADMIN_DIR = ./admin
ADMIN_NAME = popadmin
TARGET_DIR = ./bin
