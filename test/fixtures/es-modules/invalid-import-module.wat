;; Test WASM module with invalid import module name starting with 'wasm-js:'
(module
  (import "wasm-js:invalid" "test" (func $invalidImport (result i32)))
  (export "test" (func $test))
  (func $test (result i32)
    call $invalidImport
  )
)