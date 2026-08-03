(module
  (func $f (import "" "f") (param i32 i64 i64 i32) (result i32 i64 i64 i32))

  (func $g (export "g") (param i32 i64 i64 i32) (result i32 i64 i64 i32)
    (call $f (local.get 0) (local.get 2) (local.get 1) (local.get 3))
  )
)
