;; expected = R"---(;; This is a polyglot C++/WAT file.
;; Comment lines are ignored and not expected in the disassembler output.
(module
  (rec
    (type $type0 (descriptor $type1) (struct))
    (type $type1 (describes $type0) (descriptor $type2) (struct))
    (type $type2 (describes $type1) (struct))
  )
  (func $func0 (result anyref)
    ref.null none
    struct.new_default $type2
    struct.new_default $type1
    ref.cast_desc exact $type0
    ref.get_desc $type0
    struct.new_default $type2
    br_on_cast_desc 0 (ref struct) (ref $type1)
  )
)
;;)---";
