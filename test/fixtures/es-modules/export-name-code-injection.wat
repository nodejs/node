;; Compiled using the WebAssembly Binary Toolkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm export-name-code-injection.wat

(module
  (global $0 i32 (i32.const 123))
  (global $1 i32 (i32.const 456))
  (export ";import.meta.done=()=>{};console.log('code injection');{/*" (global $0))
  (export "/*/$;`//" (global $1)))
