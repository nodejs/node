;; Compiled using the WebAssembly Tootkit (https://github.com/WebAssembly/wabt)
;; $ wat2wasm test.wat -o test.wasm
(module
  (func $add (export "add") (param $first i32) (param $second i32) (result i32)
    get_local $first
    get_local $second
    (i32.add)
  )
  (export "addTwo" (func $add))
)
