(module
  (func $print (import "" "print") (param i32) (result i32))
  (func $closure (import "" "closure") (result i32))
  (func (export "run") (param $x i32) (param $y i32) (result i32)
    (i32.add
      (call $print (i32.add (local.get $x) (local.get $y)))
      (call $closure)
    )
  )
)
