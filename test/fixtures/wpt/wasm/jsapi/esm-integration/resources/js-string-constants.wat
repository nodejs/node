(module
  (import "wasm:js/string-constants" "" (global $empty externref))
  (import "wasm:js/string-constants" "\00" (global $null_byte externref))
  (import "wasm:js/string-constants" "hello" (global $hello externref))
  (import "wasm:js/string-constants" "\f0\9f\98\80" (global $emoji externref))

  (export "empty" (global $empty))
  (export "nullByte" (global $null_byte))
  (export "hello" (global $hello))
  (export "emoji" (global $emoji))
)
