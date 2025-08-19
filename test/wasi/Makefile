CC = /opt/wasi-sdk/bin/clang
TARGET = wasm32-wasi
SYSROOT = /opt/wasi-sdk/share/wasi-sysroot
CFLAGS = -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks

OBJ = $(patsubst c/%.c, wasm/%.wasm, $(wildcard c/*.c))
all: $(OBJ)

wasm/pthread.wasm : c/pthread.c
	$(CC) $< $(CFLAGS) --target=wasm32-wasi-threads -pthread -matomics -mbulk-memory -Wl,--import-memory,--export-memory,--shared-memory,--max-memory=2147483648 --sysroot=$(SYSROOT) -s -o $@

wasm/%.wasm : c/%.c
	$(CC) $< $(CFLAGS) --target=$(TARGET) --sysroot=$(SYSROOT) -s -o $@

.PHONY: clean
clean:
	rm -f $(OBJ)
