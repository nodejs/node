(module binary "\00asm\01\00\00\00")
(module binary "\00asm" "\01\00\00\00")
(module $M1 binary "\00asm\01\00\00\00")
(module $M2 binary "\00asm" "\01\00\00\00")

(assert_malformed (module binary "") "unexpected end")
(assert_malformed (module binary "\01") "unexpected end")
(assert_malformed (module binary "\00as") "unexpected end")
(assert_malformed (module binary "asm\00") "magic header not detected")
(assert_malformed (module binary "msa\00") "magic header not detected")
(assert_malformed (module binary "msa\00\01\00\00\00") "magic header not detected")
(assert_malformed (module binary "msa\00\00\00\00\01") "magic header not detected")
(assert_malformed (module binary "asm\01\00\00\00\00") "magic header not detected")
(assert_malformed (module binary "wasm\01\00\00\00") "magic header not detected")
(assert_malformed (module binary "\7fasm\01\00\00\00") "magic header not detected")
(assert_malformed (module binary "\80asm\01\00\00\00") "magic header not detected")
(assert_malformed (module binary "\82asm\01\00\00\00") "magic header not detected")
(assert_malformed (module binary "\ffasm\01\00\00\00") "magic header not detected")

;; 8-byte endian-reversed.
(assert_malformed (module binary "\00\00\00\01msa\00") "magic header not detected")

;; Middle-endian byte orderings.
(assert_malformed (module binary "a\00ms\00\01\00\00") "magic header not detected")
(assert_malformed (module binary "sm\00a\00\00\01\00") "magic header not detected")

;; Upper-cased.
(assert_malformed (module binary "\00ASM\01\00\00\00") "magic header not detected")

;; EBCDIC-encoded magic.
(assert_malformed (module binary "\00\81\a2\94\01\00\00\00") "magic header not detected")

;; Leading UTF-8 BOM.
(assert_malformed (module binary "\ef\bb\bf\00asm\01\00\00\00") "magic header not detected")

(assert_malformed (module binary "\00asm") "unexpected end")
(assert_malformed (module binary "\00asm\01") "unexpected end")
(assert_malformed (module binary "\00asm\01\00\00") "unexpected end")
(assert_malformed (module binary "\00asm\00\00\00\00") "unknown binary version")
(assert_malformed (module binary "\00asm\0d\00\00\00") "unknown binary version")
(assert_malformed (module binary "\00asm\0e\00\00\00") "unknown binary version")
(assert_malformed (module binary "\00asm\00\01\00\00") "unknown binary version")
(assert_malformed (module binary "\00asm\00\00\01\00") "unknown binary version")
(assert_malformed (module binary "\00asm\00\00\00\01") "unknown binary version")

;; Unsigned LEB128 can have non-minimal length
(module binary
  "\00asm" "\01\00\00\00"
  "\05\04\01"                          ;; Memory section with 1 entry
  "\00\82\00"                          ;; no max, minimum 2
)
(module binary
  "\00asm" "\01\00\00\00"
  "\05\07\01"                          ;; Memory section with 1 entry
  "\00\82\80\80\80\00"                 ;; no max, minimum 2
)

;; Signed LEB128 can have non-minimal length
(module binary
  "\00asm" "\01\00\00\00"
  "\06\07\01"                          ;; Global section with 1 entry
  "\7f\00"                             ;; i32, immutable
  "\41\80\00"                          ;; i32.const 0
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\07\01"                          ;; Global section with 1 entry
  "\7f\00"                             ;; i32, immutable
  "\41\ff\7f"                          ;; i32.const -1
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\0a\01"                          ;; Global section with 1 entry
  "\7f\00"                             ;; i32, immutable
  "\41\80\80\80\80\00"                 ;; i32.const 0
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\0a\01"                          ;; Global section with 1 entry
  "\7f\00"                             ;; i32, immutable
  "\41\ff\ff\ff\ff\7f"                 ;; i32.const -1
  "\0b"                                ;; end
)

(module binary
  "\00asm" "\01\00\00\00"
  "\06\07\01"                          ;; Global section with 1 entry
  "\7e\00"                             ;; i64, immutable
  "\42\80\00"                          ;; i64.const 0 with unused bits set
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\07\01"                          ;; Global section with 1 entry
  "\7e\00"                             ;; i64, immutable
  "\42\ff\7f"                          ;; i64.const -1 with unused bits unset
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\0f\01"                          ;; Global section with 1 entry
  "\7e\00"                             ;; i64, immutable
  "\42\80\80\80\80\80\80\80\80\80\00"  ;; i64.const 0 with unused bits set
  "\0b"                                ;; end
)
(module binary
  "\00asm" "\01\00\00\00"
  "\06\0f\01"                          ;; Global section with 1 entry
  "\7e\00"                             ;; i64, immutable
  "\42\ff\ff\ff\ff\ff\ff\ff\ff\ff\7f"  ;; i64.const -1 with unused bits unset
  "\0b"                                ;; end
)

;; Data segment memory index can have non-minimal length
(module binary
  "\00asm" "\01\00\00\00"
  "\05\03\01"                          ;; Memory section with 1 entry
  "\00\00"                             ;; no max, minimum 0
  "\0b\07\01"                          ;; Data section with 1 entry
  "\80\00"                             ;; Memory index 0, encoded with 2 bytes
  "\41\00\0b\00"                       ;; (i32.const 0) with contents ""
)

;; Element segment table index can have non-minimal length
(module binary
  "\00asm" "\01\00\00\00"
  "\04\04\01"                          ;; Table section with 1 entry
  "\70\00\00"                          ;; no max, minimum 0, funcref
  "\09\07\01"                          ;; Element section with 1 entry
  "\80\00"                             ;; Table index 0, encoded with 2 bytes
  "\41\00\0b\00"                       ;; (i32.const 0) with no elements
)

;; Unsigned LEB128 must not be overlong
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\08\01"                          ;; Memory section with 1 entry
    "\00\82\80\80\80\80\00"              ;; no max, minimum 2 with one byte too many
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section
    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\02"                      ;; alignment 2
    "\82\80\80\80\80\00"       ;; offset 2 with one byte too many
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section
    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\82\80\80\80\80\00"       ;; alignment 2 with one byte too many
    "\00"                      ;; offset 0
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\12\01"                ;; Code section
    ;; function 0
    "\10\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\82\80\80\80\80\00"       ;; alignment 2 with one byte too many
    "\03"                      ;; offset 3
    "\0b"                      ;; end
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\12\01"                ;; Code section
    ;; function 0
    "\10\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\02"                      ;; alignment 2
    "\82\80\80\80\80\00"       ;; offset 2 with one byte too many
    "\0b"                      ;; end
  )
  "integer representation too long"
)

;; Signed LEB128 must not be overlong
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0b\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\80\80\80\80\80\00"              ;; i32.const 0 with one byte too many
    "\0b"                                ;; end
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0b\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\ff\ff\ff\ff\ff\7f"              ;; i32.const -1 with one byte too many
    "\0b"                                ;; end
  )
  "integer representation too long"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\10\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\80\80\80\80\80\80\80\80\80\80\00"  ;; i64.const 0 with one byte too many
    "\0b"                                ;; end
  )
  "integer representation too long"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\10\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\ff\ff\ff\ff\ff\ff\ff\ff\ff\ff\7f"  ;; i64.const -1 with one byte too many
    "\0b"                                ;; end
  )
  "integer representation too long"
)

;; Unsigned LEB128s zero-extend
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\07\01"                          ;; Memory section with 1 entry
    "\00\82\80\80\80\70"                 ;; no max, minimum 2 with unused bits set
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\07\01"                          ;; Memory section with 1 entry
    "\00\82\80\80\80\40"                 ;; no max, minimum 2 with some unused bits set
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\10\01"                ;; Code section
    ;; function 0
    "\0e\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\02"                      ;; alignment 2
    "\82\80\80\80\10"          ;; offset 2 with unused bits set
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\10\01"                ;; Code section
    ;; function 0
    "\0e\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\02"                      ;; alignment 2
    "\82\80\80\80\40"          ;; offset 2 with some unused bits set
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\10\01"                ;; Code section
    "\0e\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\82\80\80\80\10"          ;; alignment 2 with unused bits set
    "\00"                      ;; offset 0
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\10\01"                ;; Code section
    ;; function 0
    "\0e\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\28"                      ;; i32.load
    "\82\80\80\80\40"          ;; alignment 2 with some unused bits set
    "\00"                      ;; offset 0
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section
    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\82\80\80\80\10"          ;; alignment 2 with unused bits set
    "\03"                      ;; offset 3
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section
    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\82\80\80\80\40"          ;; alignment 2 with some unused bits set
    "\03"                      ;; offset 3
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section
    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\03"                      ;; alignment 2
    "\82\80\80\80\10"          ;; offset 2 with unused bits set
    "\0b"                      ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\01"          ;; Memory section
    "\0a\11\01"                ;; Code section

    ;; function 0
    "\0f\01\01"                ;; local type count
    "\7f"                      ;; i32
    "\41\00"                   ;; i32.const 0
    "\41\03"                   ;; i32.const 3
    "\36"                      ;; i32.store
    "\02"                      ;; alignment 2
    "\82\80\80\80\40"          ;; offset 2 with some unused bits set
    "\0b"                      ;; end
  )
  "integer too large"
)

;; Signed LEB128s sign-extend
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0a\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\80\80\80\80\70"                 ;; i32.const 0 with unused bits set
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0a\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\ff\ff\ff\ff\0f"                 ;; i32.const -1 with unused bits unset
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0a\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\80\80\80\80\1f"                 ;; i32.const 0 with some unused bits set
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0a\01"                          ;; Global section with 1 entry
    "\7f\00"                             ;; i32, immutable
    "\41\ff\ff\ff\ff\4f"                 ;; i32.const -1 with some unused bits unset
    "\0b"                                ;; end
  )
  "integer too large"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0f\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\80\80\80\80\80\80\80\80\80\7e"  ;; i64.const 0 with unused bits set
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0f\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\ff\ff\ff\ff\ff\ff\ff\ff\ff\01"  ;; i64.const -1 with unused bits unset
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0f\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\80\80\80\80\80\80\80\80\80\02"  ;; i64.const 0 with some unused bits set
    "\0b"                                ;; end
  )
  "integer too large"
)
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\06\0f\01"                          ;; Global section with 1 entry
    "\7e\00"                             ;; i64, immutable
    "\42\ff\ff\ff\ff\ff\ff\ff\ff\ff\41"  ;; i64.const -1 with some unused bits unset
    "\0b"                                ;; end
  )
  "integer too large"
)

;; call_indirect reserved byte equal to zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\09\01"               ;; Code section

    ;; function 0
    "\07\00"
    "\41\00"                   ;; i32.const 0
    "\11\00"                   ;; call_indirect (type 0)
    "\01"                      ;; call_indirect reserved byte is not equal to zero!
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; call_indirect reserved byte should not be a "long" LEB128 zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\0a\01"               ;; Code section

    ;; function 0
    "\07\00"
    "\41\00"                   ;; i32.const 0
    "\11\00"                   ;; call_indirect (type 0)
    "\80\00"                   ;; call_indirect reserved byte
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; Same as above for 3, 4, and 5-byte zero encodings.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\0b\01"               ;; Code section

    ;; function 0
    "\08\00"
    "\41\00"                   ;; i32.const 0
    "\11\00"                   ;; call_indirect (type 0)
    "\80\80\00"                ;; call_indirect reserved byte
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\0c\01"               ;; Code section

    ;; function 0
    "\09\00"
    "\41\00"                   ;; i32.const 0
    "\11\00"                   ;; call_indirect (type 0)
    "\80\80\80\00"             ;; call_indirect reserved byte
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\0d\01"               ;; Code section

    ;; function 0
    "\0a\00"
    "\41\00"                   ;; i32.const 0
    "\11\00"                   ;; call_indirect (type 0)
    "\80\80\80\80\00"          ;; call_indirect reserved byte
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; memory.grow reserved byte equal to zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\09\01"                ;; Code section

    ;; function 0
    "\07\00"
    "\41\00"                   ;; i32.const 0
    "\40"                      ;; memory.grow
    "\01"                      ;; memory.grow reserved byte is not equal to zero!
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; memory.grow reserved byte should not be a "long" LEB128 zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0a\01"                ;; Code section

    ;; function 0
    "\08\00"
    "\41\00"                   ;; i32.const 0
    "\40"                      ;; memory.grow
    "\80\00"                   ;; memory.grow reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; Same as above for 3, 4, and 5-byte zero encodings.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0b\01"                ;; Code section

    ;; function 0
    "\09\00"
    "\41\00"                   ;; i32.const 0
    "\40"                      ;; memory.grow
    "\80\80\00"                ;; memory.grow reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0c\01"                ;; Code section

    ;; function 0
    "\0a\00"
    "\41\00"                   ;; i32.const 0
    "\40"                      ;; memory.grow
    "\80\80\80\00"             ;; memory.grow reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0d\01"                ;; Code section

    ;; function 0
    "\0b\00"
    "\41\00"                   ;; i32.const 0
    "\40"                      ;; memory.grow
    "\80\80\80\80\00"          ;; memory.grow reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; memory.size reserved byte equal to zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\07\01"                ;; Code section

    ;; function 0
    "\05\00"
    "\3f"                      ;; memory.size
    "\01"                      ;; memory.size reserved byte is not equal to zero!
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; memory.size reserved byte should not be a "long" LEB128 zero.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\08\01"                ;; Code section

    ;; function 0
    "\06\00"
    "\3f"                      ;; memory.size
    "\80\00"                   ;; memory.size reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; Same as above for 3, 4, and 5-byte zero encodings.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\09\01"                ;; Code section

    ;; function 0
    "\07\00"
    "\3f"                      ;; memory.size
    "\80\80\00"                ;; memory.size reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0a\01"                ;; Code section

    ;; function 0
    "\08\00"
    "\3f"                      ;; memory.size
    "\80\80\80\00"             ;; memory.size reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0b\01"                ;; Code section

    ;; function 0
    "\09\00"
    "\3f"                      ;; memory.size
    "\80\80\80\80\00"          ;; memory.size reserved byte
    "\1a"                      ;; drop
    "\0b"                      ;; end
  )
  "zero flag expected"
)

;; No more than 2^32 locals.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\0a\0c\01"                ;; Code section

    ;; function 0
    "\0a\02"
    "\ff\ff\ff\ff\0f\7f"       ;; 0xFFFFFFFF i32
    "\02\7e"                   ;; 0x00000002 i64
    "\0b"                      ;; end
  )
  "too many locals"
)

;; Local count can be 0.
(module binary
  "\00asm" "\01\00\00\00"
  "\01\04\01\60\00\00"     ;; Type section
  "\03\02\01\00"           ;; Function section
  "\0a\0a\01"              ;; Code section

  ;; function 0
  "\08\03"
  "\00\7f"                 ;; 0 i32
  "\00\7e"                 ;; 0 i64
  "\02\7d"                 ;; 2 f32
  "\0b"                    ;; end
)

;; Function section has non-zero count, but code section is absent.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"  ;; Type section
    "\03\03\02\00\00"     ;; Function section with 2 functions
  )
  "function and code section have inconsistent lengths"
)

;; Code section has non-zero count, but function section is absent.
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\0a\04\01\02\00\0b"  ;; Code section with 1 empty function
  )
  "function and code section have inconsistent lengths"
)

;; Function section count > code section count
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"  ;; Type section
    "\03\03\02\00\00"     ;; Function section with 2 functions
    "\0a\04\01\02\00\0b"  ;; Code section with 1 empty function
  )
  "function and code section have inconsistent lengths"
)

;; Function section count < code section count
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"           ;; Type section
    "\03\02\01\00"                 ;; Function section with 1 function
    "\0a\07\02\02\00\0b\02\00\0b"  ;; Code section with 2 empty functions
  )
  "function and code section have inconsistent lengths"
)

;; Function section has zero count, and code section is absent.
(module binary
  "\00asm" "\01\00\00\00"
  "\03\01\00"  ;; Function section with 0 functions
)

;; Code section has zero count, and function section is absent.
(module binary
  "\00asm" "\01\00\00\00"
  "\0a\01\00"  ;; Code section with 0 functions
)

;; Fewer passive segments than datacount
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\0c\01\03"                   ;; Datacount section with value "3"
    "\0b\05\02"                   ;; Data section with two entries
    "\01\00"                      ;; Passive data section
    "\01\00")                     ;; Passive data section
  "data count and data section have inconsistent lengths")

;; More passive segments than datacount
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\0c\01\01"                   ;; Datacount section with value "1"
    "\0b\05\02"                   ;; Data section with two entries
    "\01\00"                      ;; Passive data section
    "\01\00")                     ;; Passive data section
  "data count and data section have inconsistent lengths")

;; memory.init requires a datacount section
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\0e\01"                ;; Code section

    ;; function 0
    "\0c\00"
    "\41\00"                   ;; zero args
    "\41\00"
    "\41\00"
    "\fc\08\00\00"             ;; memory.init
    "\0b"

    "\0b\03\01\01\00"          ;; Data section
  )                            ;; end
  "data count section required")

;; data.drop requires a datacount section
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

    "\01\04\01\60\00\00"       ;; Type section
    "\03\02\01\00"             ;; Function section
    "\05\03\01\00\00"          ;; Memory section
    "\0a\07\01"                ;; Code section

    ;; function 0
    "\05\00"
    "\fc\09\00"                ;; data.drop
    "\0b"

    "\0b\03\01\01\00"          ;; Data section
  )                            ;; end
  "data count section required")

;; passive element segment containing opcode other than ref.func or ref.null
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

    "\01\04\01\60\00\00"       ;; Type section

    "\03\02\01\00"             ;; Function section

    "\04\04\01"                ;; Table section with 1 entry
    "\70\00\00"                ;; no max, minimum 0, funcref

    "\05\03\01\00\00"          ;; Memory section

    "\09\07\01"                ;; Element section with one segment
    "\01\70"                   ;; Passive, funcref
    "\01"                      ;; 1 element
    "\d3\00\0b"                ;; bad opcode, index 0, end

    "\0a\04\01"                ;; Code section

    ;; function 0
    "\02\00"
    "\0b")                     ;; end
  "invalid elem")

;; passive element segment containing type other than funcref
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

    "\01\04\01\60\00\00"       ;; Type section

    "\03\02\01\00"             ;; Function section

    "\04\04\01"                ;; Table section with 1 entry
    "\70\00\00"                ;; no max, minimum 0, funcref

    "\05\03\01\00\00"          ;; Memory section

    "\09\07\01"                ;; Element section with one segment
    "\01\7f"                   ;; Passive, i32
    "\01"                      ;; 1 element
    "\d2\00\0b"                ;; ref.func, index 0, end

    "\0a\04\01"                ;; Code section

    ;; function 0
    "\02\00"
    "\0b")                     ;; end
  "invalid element type")

;; passive element segment containing opcode ref.func
(module binary
  "\00asm" "\01\00\00\00"

  "\01\04\01\60\00\00"       ;; Type section

  "\03\02\01\00"             ;; Function section

  "\04\04\01"                ;; Table section with 1 entry
  "\70\00\00"                ;; no max, minimum 0, funcref

  "\05\03\01\00\00"          ;; Memory section

  "\09\07\01"                ;; Element section with one segment
  "\01\70"                   ;; Passive, funcref
  "\01"                      ;; 1 element
  "\d2\00\0b"                ;; ref.func, index 0, end

  "\0a\04\01"                ;; Code section

  ;; function 0
  "\02\00"
  "\0b")                     ;; end

;; passive element segment containing opcode ref.null
(module binary
  "\00asm" "\01\00\00\00"

  "\01\04\01\60\00\00"       ;; Type section

  "\03\02\01\00"             ;; Function section

  "\04\04\01"                ;; Table section with 1 entry
  "\70\00\00"                ;; no max, minimum 0, funcref

  "\05\03\01\00\00"          ;; Memory section

  "\09\06\01"                ;; Element section with one segment
  "\01\70"                   ;; Passive, funcref
  "\01"                      ;; 1 element
  "\d0\0b"                   ;; ref.null, end

  "\0a\04\01"                ;; Code section

  ;; function 0
  "\02\00"
  "\0b")                     ;; end
