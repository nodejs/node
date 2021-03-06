CC = /opt/wasi-sdk/bin/clang
TARGET = wasm32-unknown-wasi
SYSROOT =

OBJ = $(patsubst c/%.c, wasm/%.wasm, $(wildcard c/*.c))
all: $(OBJ)

wasm/%.wasm : c/%.c
	$(CC) $< --target=$(TARGET) --sysroot=$(SYSROOT) -s -o $@

.PHONY clean:
	rm -f $(OBJ)
