CC = /opt/wasi-sdk/bin/clang
TARGET = wasm32-wasi
SYSROOT =
CFLAGS = -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks

OBJ = $(patsubst c/%.c, wasm/%.wasm, $(wildcard c/*.c))
all: $(OBJ)

wasm/%.wasm : c/%.c
	$(CC) $< $(CFLAGS) --target=$(TARGET) --sysroot=$(SYSROOT) -s -o $@

.PHONY clean:
	rm -f $(OBJ)
