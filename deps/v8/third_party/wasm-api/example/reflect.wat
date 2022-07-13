(module
  (func (export "func") (param i32 f64 f32) (result i32) (unreachable))
  (global (export "global") f64 (f64.const 0))
  (table (export "table") 0 50 anyfunc)
  (memory (export "memory") 1)
)
