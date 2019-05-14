;; Compiled using the WebAssembly Tootkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm simple.wat -o simple.wasm

(module
  (func $add (param $a i32) (param $b i32) (result i32)
    ;; return $a + $b
    (i32.add (get_local $a) (get_local $b))
  )

  (export "add" (func $add))
)
