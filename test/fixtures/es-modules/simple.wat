;; Compiled using the WebAssembly Tootkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm simple.wat -o simple.wasm

(module
  (import "./wasm-dep.mjs" "jsFn" (func $jsFn (result i32)))
  (import "./wasm-dep.mjs" "jsInitFn" (func $jsInitFn))
  (export "add" (func $add))
  (export "addImported" (func $addImported))
  (start $startFn)
  (func $startFn
    call $jsInitFn
  )
  (func $add (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.add
  )
  (func $addImported (param $a i32) (result i32)
    local.get $a
    call $jsFn
    i32.add
  )
)
