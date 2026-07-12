(module
  (func $callback (import "" "callback") (result i32))
  (func (export "callback") (result i32) (call $callback))
  (func (export "unreachable") (result i32) (unreachable) (i32.const 1))
)
