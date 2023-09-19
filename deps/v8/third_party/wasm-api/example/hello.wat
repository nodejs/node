(module
  (func $hello (import "" "hello"))
  (func (export "run") (call $hello))
)
