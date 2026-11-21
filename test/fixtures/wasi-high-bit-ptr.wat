;; Build: wasm-tools parse wasi-high-bit-ptr.wat -o wasi-high-bit-ptr.wasm
(module
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "args_sizes_get"
    (func $args_sizes_get (param i32 i32) (result i32)))
  (memory (export "memory") 1)
  (func (export "_start"))
  (func (export "clock_time_get_high_ptr") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0x80000000
    call $clock_time_get)
  (func (export "clock_time_get_low_ptr") (result i32)
    i32.const 0
    i64.const 1
    i32.const 8
    call $clock_time_get)
  (func (export "args_sizes_get_high_ptr") (result i32)
    i32.const 0x80000000
    i32.const 0xfffffff0
    call $args_sizes_get))
