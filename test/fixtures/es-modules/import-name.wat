;; Compiled using the WebAssembly Binary Toolkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm import-name.wat

(module
  (global $0 (import "./export-name-code-injection.wasm" ";import.meta.done=()=>{};console.log('code injection');{/*") i32)
  (global $1 (import "./export-name-code-injection.wasm" "/*/$;`//") i32)
  (global $2 (import "./export-name-syntax-error.wasm" "?f!o:o<b>a[r]") i32)
  (func $xor (result i32)
    (i32.xor (i32.xor (global.get $0) (global.get $1)) (global.get $2)))
  (export "xor" (func $xor)))
