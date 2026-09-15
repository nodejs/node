;; expected = R"---(;; This is a polyglot C++/WAT file.
;; Comment lines are ignored and not expected in the disassembler output.
(module
  (memory $$myMemory0 (;0;) (export "$myMemory0") 1 1 shared)
  (memory $$myMemory1 (;1;) (export "$myMemory1") 1 1 shared)
  (func $test (;0;) (export "test")
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    i32.atomic.load acqrel offset=1
    i32.atomic.rmw.or $$myMemory1 acqrel offset=2
    i32.atomic.store $$myMemory1 acqrel offset=3
    i32.atomic.load offset=1
    i32.atomic.rmw.or $$myMemory1 offset=2
    i32.atomic.store $$myMemory1 offset=3
    i32.atomic.load offset=1
    i32.atomic.rmw.or $$myMemory1 offset=2
    i32.atomic.store $$myMemory1 offset=3
    i32.atomic.load acqrel offset=1
    i32.atomic.rmw.or acqrel offset=2
    i32.atomic.store acqrel offset=3
  )
)
;;)---";
