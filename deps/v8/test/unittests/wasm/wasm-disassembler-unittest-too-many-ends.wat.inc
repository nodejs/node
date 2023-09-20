;; expected = R"---(;; This is a polyglot C++/WAT file.
(module
  (func $doubleEnd (;0;) (export "doubleEnd")
    nop
  )
  ;; Unexpected end byte
  ;; Unexpected end byte
)
;;)---";
