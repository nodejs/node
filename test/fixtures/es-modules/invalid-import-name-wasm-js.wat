;; Test WASM module with invalid import name starting with 'wasm-js:'
(module
  (import "test" "wasm-js:invalid" (func $invalidImport (result i32)))
  (export "test" (func $test))
  (func $test (result i32)
    call $invalidImport
  )
)