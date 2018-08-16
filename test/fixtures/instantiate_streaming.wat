;; $ wat2wasm instantiate_streaming.wat -o instantiate_streaming.wasm
(module
 (func $add (import "test" "add") (param i32) (param i32) (result i32))
  (func (export "add") (param i32) (param i32) (result i32)
    get_local 0
    get_local 1
    call $add
   )
)
