;; expected = R"---(;; This is a polyglot C++/WAT file.
;; Comment lines are ignored and not expected in the disassembler output.
(module
  (type $type0 (array (field (mut i32))))
  (func $func0 (result i32)
    i32.const 1
    i32.const 2
    array.new $type0
    i32.const 0
    i32.const 42
    array.atomic.rmw.xor acqrel $type0
  )
)
;;)---";
