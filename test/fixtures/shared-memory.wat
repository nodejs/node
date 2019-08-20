;; Compiled using the WebAssembly Tootkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm --enable-threads shared-memory.wat -o shared-memory.wasm

(module
  ;; Create shared memory with initial 1 page (64KiB) and max 1 page
  (memory $mem 1 1 shared)
  (export "memory" (memory $mem))
)