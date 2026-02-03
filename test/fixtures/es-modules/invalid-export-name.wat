;; Test WASM module with invalid export name starting with 'wasm:'
(module
  (func $test (result i32)
    i32.const 42
  )
  (export "wasm:invalid" (func $test))
)