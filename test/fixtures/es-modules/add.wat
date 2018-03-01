;; source of add.wasm

(module
  (import "./add.mjs" "add" (func $add_i32 (param i32 i32) (result i32)))

  (func $add (param $p0 i32) (param $p1 i32) (result i32)
    get_local $p0
    get_local $p1
    call $add_i32)

  (func $add_10 (param $p0 i32) (result i32)
    get_local $p0
    i32.const 10
    i32.add)

  (export "add" (func $add))
  (export "default" (func $add_10)))

