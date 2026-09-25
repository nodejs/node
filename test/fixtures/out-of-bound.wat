(module
 (type $none_=>_none (func))
 (memory $0 1)
 (export "_start" (func $_start))
 (func $_start
  memory.size
  i32.const 64
  i32.mul
  i32.const 1024
  i32.mul
  i32.const 3
  i32.sub
  i32.load
  drop
 )
)
