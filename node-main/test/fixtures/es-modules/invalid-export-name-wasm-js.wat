;; Test WASM module with invalid export name starting with 'wasm-js:'
(module
  (func $test (result i32)
    i32.const 42
  )
  (export "wasm-js:invalid" (func $test))
)