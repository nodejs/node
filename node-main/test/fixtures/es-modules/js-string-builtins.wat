;; Test WASM module using js-string builtins
(module
  ;; Import js-string builtins with correct signatures
  (import "wasm:js-string" "length" (func $string_length (param externref) (result i32)))
  (import "wasm:js-string" "concat" (func $string_concat (param externref externref) (result (ref extern))))
  (import "wasm:js-string" "equals" (func $string_equals (param externref externref) (result i32)))
  
  ;; Export functions that use the builtins
  (export "getLength" (func $get_length))
  (export "concatStrings" (func $concat_strings))
  (export "compareStrings" (func $compare_strings))
  
  (func $get_length (param $str externref) (result i32)
    local.get $str
    call $string_length
  )
  
  (func $concat_strings (param $str1 externref) (param $str2 externref) (result (ref extern))
    local.get $str1
    local.get $str2
    call $string_concat
  )
  
  (func $compare_strings (param $str1 externref) (param $str2 externref) (result i32)
    local.get $str1
    local.get $str2
    call $string_equals
  )
)