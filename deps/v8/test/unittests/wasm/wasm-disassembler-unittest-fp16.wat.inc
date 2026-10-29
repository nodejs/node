;; expected = R"---(;; This is a polyglot C++/WAT file.
;; Comment lines are ignored and not expected in the disassembler output.
(module
  (memory $memory0 1 1)
  (func $func0 (result f32)
    i32.const 0
    f32.load_f16
    i32.const 0
    f32.load_f16 offset=4
    f32.add
    i32.const 0
    f32.load_f16 align=1
    f32.add
  )
  (func $func1 (param $var0 f32)
    i32.const 0
    local.get $var0
    f32.store_f16
    i32.const 0
    local.get $var0
    f32.store_f16 offset=4
    i32.const 0
    local.get $var0
    f32.store_f16 align=1
  )
)
;;)---";
