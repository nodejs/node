;; Functions

(module $Mf
  (func (export "call") (result i32) (call $g))
  (func $g (result i32) (i32.const 2))
)
(register "Mf" $Mf)

(module $Nf
  (func $f (import "Mf" "call") (result i32))
  (export "Mf.call" (func $f))
  (func (export "call Mf.call") (result i32) (call $f))
  (func (export "call") (result i32) (call $g))
  (func $g (result i32) (i32.const 3))
)

(assert_return (invoke $Mf "call") (i32.const 2))
(assert_return (invoke $Nf "Mf.call") (i32.const 2))
(assert_return (invoke $Nf "call") (i32.const 3))
(assert_return (invoke $Nf "call Mf.call") (i32.const 2))

(module
  (import "spectest" "print_i32" (func $f (param i32)))
  (export "print" (func $f))
)
(register "reexport_f")
(assert_unlinkable
  (module (import "reexport_f" "print" (func (param i64))))
  "incompatible import type"
)
(assert_unlinkable
  (module (import "reexport_f" "print" (func (param i32) (result i32))))
  "incompatible import type"
)


;; Globals

(module $Mg
  (global $glob (export "glob") i32 (i32.const 42))
  (func (export "get") (result i32) (global.get $glob))

  ;; export mutable globals
  (global $mut_glob (export "mut_glob") (mut i32) (i32.const 142))
  (func (export "get_mut") (result i32) (global.get $mut_glob))
  (func (export "set_mut") (param i32) (global.set $mut_glob (local.get 0)))
)
(register "Mg" $Mg)

(module $Ng
  (global $x (import "Mg" "glob") i32)
  (global $mut_glob (import "Mg" "mut_glob") (mut i32))
  (func $f (import "Mg" "get") (result i32))
  (func $get_mut (import "Mg" "get_mut") (result i32))
  (func $set_mut (import "Mg" "set_mut") (param i32))

  (export "Mg.glob" (global $x))
  (export "Mg.get" (func $f))
  (global $glob (export "glob") i32 (i32.const 43))
  (func (export "get") (result i32) (global.get $glob))

  (export "Mg.mut_glob" (global $mut_glob))
  (export "Mg.get_mut" (func $get_mut))
  (export "Mg.set_mut" (func $set_mut))
)

(assert_return (get $Mg "glob") (i32.const 42))
(assert_return (get $Ng "Mg.glob") (i32.const 42))
(assert_return (get $Ng "glob") (i32.const 43))
(assert_return (invoke $Mg "get") (i32.const 42))
(assert_return (invoke $Ng "Mg.get") (i32.const 42))
(assert_return (invoke $Ng "get") (i32.const 43))

(assert_return (get $Mg "mut_glob") (i32.const 142))
(assert_return (get $Ng "Mg.mut_glob") (i32.const 142))
(assert_return (invoke $Mg "get_mut") (i32.const 142))
(assert_return (invoke $Ng "Mg.get_mut") (i32.const 142))

(assert_return (invoke $Mg "set_mut" (i32.const 241)))
(assert_return (get $Mg "mut_glob") (i32.const 241))
(assert_return (get $Ng "Mg.mut_glob") (i32.const 241))
(assert_return (invoke $Mg "get_mut") (i32.const 241))
(assert_return (invoke $Ng "Mg.get_mut") (i32.const 241))


(assert_unlinkable
  (module (import "Mg" "mut_glob" (global i32)))
  "incompatible import type"
)
(assert_unlinkable
  (module (import "Mg" "glob" (global (mut i32))))
  "incompatible import type"
)

;; Tables

(module $Mt
  (type (func (result i32)))
  (type (func))

  (table (export "tab") 10 funcref)
  (elem (i32.const 2) $g $g $g $g)
  (func $g (result i32) (i32.const 4))
  (func (export "h") (result i32) (i32.const -4))

  (func (export "call") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0))
  )
)
(register "Mt" $Mt)

(module $Nt
  (type (func))
  (type (func (result i32)))

  (func $f (import "Mt" "call") (param i32) (result i32))
  (func $h (import "Mt" "h") (result i32))

  (table funcref (elem $g $g $g $h $f))
  (func $g (result i32) (i32.const 5))

  (export "Mt.call" (func $f))
  (func (export "call Mt.call") (param i32) (result i32)
    (call $f (local.get 0))
  )
  (func (export "call") (param i32) (result i32)
    (call_indirect (type 1) (local.get 0))
  )
)

(assert_return (invoke $Mt "call" (i32.const 2)) (i32.const 4))
(assert_return (invoke $Nt "Mt.call" (i32.const 2)) (i32.const 4))
(assert_return (invoke $Nt "call" (i32.const 2)) (i32.const 5))
(assert_return (invoke $Nt "call Mt.call" (i32.const 2)) (i32.const 4))

(assert_trap (invoke $Mt "call" (i32.const 1)) "uninitialized")
(assert_trap (invoke $Nt "Mt.call" (i32.const 1)) "uninitialized")
(assert_return (invoke $Nt "call" (i32.const 1)) (i32.const 5))
(assert_trap (invoke $Nt "call Mt.call" (i32.const 1)) "uninitialized")

(assert_trap (invoke $Mt "call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $Nt "Mt.call" (i32.const 0)) "uninitialized")
(assert_return (invoke $Nt "call" (i32.const 0)) (i32.const 5))
(assert_trap (invoke $Nt "call Mt.call" (i32.const 0)) "uninitialized")

(assert_trap (invoke $Mt "call" (i32.const 20)) "undefined")
(assert_trap (invoke $Nt "Mt.call" (i32.const 20)) "undefined")
(assert_trap (invoke $Nt "call" (i32.const 7)) "undefined")
(assert_trap (invoke $Nt "call Mt.call" (i32.const 20)) "undefined")

(assert_return (invoke $Nt "call" (i32.const 3)) (i32.const -4))
(assert_trap (invoke $Nt "call" (i32.const 4)) "indirect call")

(module $Ot
  (type (func (result i32)))

  (func $h (import "Mt" "h") (result i32))
  (table (import "Mt" "tab") 5 funcref)
  (elem (i32.const 1) $i $h)
  (func $i (result i32) (i32.const 6))

  (func (export "call") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0))
  )
)

(assert_return (invoke $Mt "call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $Nt "Mt.call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $Nt "call Mt.call" (i32.const 3)) (i32.const 4))
(assert_return (invoke $Ot "call" (i32.const 3)) (i32.const 4))

(assert_return (invoke $Mt "call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $Nt "Mt.call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $Nt "call" (i32.const 2)) (i32.const 5))
(assert_return (invoke $Nt "call Mt.call" (i32.const 2)) (i32.const -4))
(assert_return (invoke $Ot "call" (i32.const 2)) (i32.const -4))

(assert_return (invoke $Mt "call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $Nt "Mt.call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $Nt "call" (i32.const 1)) (i32.const 5))
(assert_return (invoke $Nt "call Mt.call" (i32.const 1)) (i32.const 6))
(assert_return (invoke $Ot "call" (i32.const 1)) (i32.const 6))

(assert_trap (invoke $Mt "call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $Nt "Mt.call" (i32.const 0)) "uninitialized")
(assert_return (invoke $Nt "call" (i32.const 0)) (i32.const 5))
(assert_trap (invoke $Nt "call Mt.call" (i32.const 0)) "uninitialized")
(assert_trap (invoke $Ot "call" (i32.const 0)) "uninitialized")

(assert_trap (invoke $Ot "call" (i32.const 20)) "undefined")

(module
  (table (import "Mt" "tab") 0 funcref)
  (elem (i32.const 9) $f)
  (func $f)
)

(module $G1 (global (export "g") i32 (i32.const 5)))
(register "G1" $G1)
(module $G2
  (global (import "G1" "g") i32)
  (global (export "g") i32 (global.get 0))
)
(assert_return (get $G2 "g") (i32.const 5))

(assert_unlinkable
  (module
    (table (import "Mt" "tab") 0 funcref)
    (elem (i32.const 10) $f)
    (func $f)
  )
  "elements segment does not fit"
)

(assert_unlinkable
  (module
    (table (import "Mt" "tab") 10 funcref)
    (memory (import "Mt" "mem") 1)  ;; does not exist
    (func $f (result i32) (i32.const 0))
    (elem (i32.const 7) $f)
    (elem (i32.const 9) $f)
  )
  "unknown import"
)
(assert_trap (invoke $Mt "call" (i32.const 7)) "uninitialized")

;; Unlike in the v1 spec, the elements stored before an out-of-bounds access
;; persist after the instantiation failure.
(assert_unlinkable
  (module
    (table (import "Mt" "tab") 10 funcref)
    (func $f (result i32) (i32.const 0))
    (elem (i32.const 7) $f)
    (elem (i32.const 12) $f)  ;; out of bounds
  )
  "elements segment does not fit"
)
(assert_return (invoke $Mt "call" (i32.const 7)) (i32.const 0))

(assert_unlinkable
  (module
    (table (import "Mt" "tab") 10 funcref)
    (func $f (result i32) (i32.const 0))
    (elem (i32.const 7) $f)
    (memory 1)
    (data (i32.const 0x10000) "d") ;; out of bounds
  )
  "data segment does not fit"
)
(assert_return (invoke $Mt "call" (i32.const 7)) (i32.const 0))


;; Memories

(module $Mm
  (memory (export "mem") 1 5)
  (data (i32.const 10) "\00\01\02\03\04\05\06\07\08\09")

  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)
(register "Mm" $Mm)

(module $Nm
  (func $loadM (import "Mm" "load") (param i32) (result i32))

  (memory 1)
  (data (i32.const 10) "\f0\f1\f2\f3\f4\f5")

  (export "Mm.load" (func $loadM))
  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)

(assert_return (invoke $Mm "load" (i32.const 12)) (i32.const 2))
(assert_return (invoke $Nm "Mm.load" (i32.const 12)) (i32.const 2))
(assert_return (invoke $Nm "load" (i32.const 12)) (i32.const 0xf2))

(module $Om
  (memory (import "Mm" "mem") 1)
  (data (i32.const 5) "\a0\a1\a2\a3\a4\a5\a6\a7")

  (func (export "load") (param $a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)

(assert_return (invoke $Mm "load" (i32.const 12)) (i32.const 0xa7))
(assert_return (invoke $Nm "Mm.load" (i32.const 12)) (i32.const 0xa7))
(assert_return (invoke $Nm "load" (i32.const 12)) (i32.const 0xf2))
(assert_return (invoke $Om "load" (i32.const 12)) (i32.const 0xa7))

(module
  (memory (import "Mm" "mem") 0)
  (data (i32.const 0xffff) "a")
)

(assert_unlinkable
  (module
    (memory (import "Mm" "mem") 0)
    (data (i32.const 0x10000) "a")
  )
  "data segment does not fit"
)

(module $Pm
  (memory (import "Mm" "mem") 1 8)

  (func (export "grow") (param $a i32) (result i32)
    (memory.grow (local.get 0))
  )
)

(assert_return (invoke $Pm "grow" (i32.const 0)) (i32.const 1))
(assert_return (invoke $Pm "grow" (i32.const 2)) (i32.const 1))
(assert_return (invoke $Pm "grow" (i32.const 0)) (i32.const 3))
(assert_return (invoke $Pm "grow" (i32.const 1)) (i32.const 3))
(assert_return (invoke $Pm "grow" (i32.const 1)) (i32.const 4))
(assert_return (invoke $Pm "grow" (i32.const 0)) (i32.const 5))
(assert_return (invoke $Pm "grow" (i32.const 1)) (i32.const -1))
(assert_return (invoke $Pm "grow" (i32.const 0)) (i32.const 5))

(assert_unlinkable
  (module
    (func $host (import "spectest" "print"))
    (memory (import "Mm" "mem") 1)
    (table (import "Mm" "tab") 0 funcref)  ;; does not exist
    (data (i32.const 0) "abc")
  )
  "unknown import"
)
(assert_return (invoke $Mm "load" (i32.const 0)) (i32.const 0))

;; Unlike in v1 spec, bytes written before an out-of-bounds access persist
;; after the instantiation failure.
(assert_unlinkable
  (module
    (memory (import "Mm" "mem") 1)
    (data (i32.const 0) "abc")
    (data (i32.const 0x50000) "d") ;; out of bounds
  )
  "data segment does not fit"
)
(assert_return (invoke $Mm "load" (i32.const 0)) (i32.const 97))

(assert_unlinkable
  (module
    (memory (import "Mm" "mem") 1)
    (data (i32.const 0) "abc")
    (table 0 funcref)
    (func)
    (elem (i32.const 0) 0) ;; out of bounds
  )
  "elements segment does not fit"
)
(assert_return (invoke $Mm "load" (i32.const 0)) (i32.const 97))

;; Store is modified if the start function traps.
(module $Ms
  (type $t (func (result i32)))
  (memory (export "memory") 1)
  (table (export "table") 1 funcref)
  (func (export "get memory[0]") (type $t)
    (i32.load8_u (i32.const 0))
  )
  (func (export "get table[0]") (type $t)
    (call_indirect (type $t) (i32.const 0))
  )
)
(register "Ms" $Ms)

(assert_trap
  (module
    (import "Ms" "memory" (memory 1))
    (import "Ms" "table" (table 1 funcref))
    (data (i32.const 0) "hello")
    (elem (i32.const 0) $f)
    (func $f (result i32)
      (i32.const 0xdead)
    )
    (func $main
      (unreachable)
    )
    (start $main)
  )
  "unreachable"
)

(assert_return (invoke $Ms "get memory[0]") (i32.const 104))  ;; 'h'
(assert_return (invoke $Ms "get table[0]") (i32.const 0xdead))
