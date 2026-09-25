(module
  (import "wasi_snapshot_preview1" "clock_time_get"
    (func $clock_time_get (param i32 i64 i32) (result i32)))

  (memory (export "memory") 2)

  (func (export "test_clock_time_get") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0x80000000
    call $clock_time_get
  )

  (func (export "test_clock_time_get_zero") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0
    call $clock_time_get
  )

  (func (export "test_clock_time_get_one") (result i32)
    i32.const 1
    i64.const 1
    i32.const 1
    call $clock_time_get
  )

  (func (export "test_clock_time_get_int32_max") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0x7fffffff
    call $clock_time_get
  )

  (func (export "test_clock_time_get_uint32_min") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0x80000000
    call $clock_time_get
  )

  (func (export "test_clock_time_get_uint32_max") (result i32)
    i32.const 0
    i64.const 1
    i32.const 0xffffffff
    call $clock_time_get
  )

  (func (export "_start"))
)
