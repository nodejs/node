(module
  (type (;0;) (func (result i32)))
  (type (;1;) (func (result i64)))
  (type (;2;) (func (result f32)))
  (type (;3;) (func (result f64)))
  (type (;4;) (func (result externref)))
  (type (;5;) (func (result v128)))
  (type (;6;) (func (param i32)))
  (type (;7;) (func (param i64)))
  (type (;8;) (func (param f32)))
  (type (;9;) (func (param f64)))
  (type (;10;) (func (param externref)))
  (type (;11;) (func (param v128)))
  (import "./globals.js" "\u{1f680}i32_value" (global $imported_i32 (;0;) i32))
  (import "./globals.js" "i32_mut_value" (global $imported_mut_i32 (;1;) (mut i32)))
  (import "./globals.js" "i64_value" (global $imported_i64 (;2;) i64))
  (import "./globals.js" "i64_mut_value" (global $imported_mut_i64 (;3;) (mut i64)))
  (import "./globals.js" "f32_value" (global $imported_f32 (;4;) f32))
  (import "./globals.js" "f32_mut_value" (global $imported_mut_f32 (;5;) (mut f32)))
  (import "./globals.js" "f64_value" (global $imported_f64 (;6;) f64))
  (import "./globals.js" "f64_mut_value" (global $imported_mut_f64 (;7;) (mut f64)))
  (import "./globals.js" "externref_value" (global $imported_externref (;8;) externref))
  (import "./globals.js" "externref_mut_value" (global $imported_mut_externref (;9;) (mut externref)))
  (import "./globals.js" "null_externref_value" (global $imported_null_externref (;10;) externref))
  (import "./dep.wasm" "i32_value" (global $dep_i32 (;11;) i32))
  (import "./dep.wasm" "i32_mut_value" (global $dep_mut_i32 (;12;) (mut i32)))
  (import "./dep.wasm" "i64_value" (global $dep_i64 (;13;) i64))
  (import "./dep.wasm" "i64_mut_value" (global $dep_mut_i64 (;14;) (mut i64)))
  (import "./dep.wasm" "f32_value" (global $dep_f32 (;15;) f32))
  (import "./dep.wasm" "f32_mut_value" (global $dep_mut_f32 (;16;) (mut f32)))
  (import "./dep.wasm" "f64_value" (global $dep_f64 (;17;) f64))
  (import "./dep.wasm" "f64_mut_value" (global $dep_mut_f64 (;18;) (mut f64)))
  (import "./dep.wasm" "externref_value" (global $dep_externref (;19;) externref))
  (import "./dep.wasm" "externref_mut_value" (global $dep_mut_externref (;20;) (mut externref)))
  (import "./dep.wasm" "v128_value" (global $dep_v128 (;21;) v128))
  (import "./dep.wasm" "v128_mut_value" (global $dep_mut_v128 (;22;) (mut v128)))
  (global $local_i32 (;23;) i32 i32.const 42)
  (global $local_mut_i32 (;24;) (mut i32) i32.const 100)
  (global $local_i64 (;25;) i64 i64.const 9223372036854775807)
  (global $local_mut_i64 (;26;) (mut i64) i64.const 200)
  (global $local_f32 (;27;) f32 f32.const 0x1.921fap+1 (;=3.14159;))
  (global $local_mut_f32 (;28;) (mut f32) f32.const 0x1.5bf09ap+1 (;=2.71828;))
  (global $local_f64 (;29;) f64 f64.const 0x1.5bf0a8b145769p+1 (;=2.718281828459045;))
  (global $local_mut_f64 (;30;) (mut f64) f64.const 0x1.921fb54442d18p+1 (;=3.141592653589793;))
  (global $local_externref (;31;) externref ref.null extern)
  (global $local_mut_externref (;32;) (mut externref) ref.null extern)
  (global $local_v128 (;33;) v128 v128.const i32x4 0x00000001 0x00000002 0x00000003 0x00000004)
  (global $local_mut_v128 (;34;) (mut v128) v128.const i32x4 0x00000005 0x00000006 0x00000007 0x00000008)
  (export "importedI32" (global $imported_i32))
  (export "importedMutI32" (global $imported_mut_i32))
  (export "importedI64" (global $imported_i64))
  (export "importedMutI64" (global $imported_mut_i64))
  (export "importedF32" (global $imported_f32))
  (export "importedMutF32" (global $imported_mut_f32))
  (export "importedF64" (global $imported_f64))
  (export "importedMutF64" (global $imported_mut_f64))
  (export "importedExternref" (global $imported_externref))
  (export "importedMutExternref" (global $imported_mut_externref))
  (export "importedNullExternref" (global $imported_null_externref))
  (export "depI32" (global $dep_i32))
  (export "depMutI32" (global $dep_mut_i32))
  (export "depI64" (global $dep_i64))
  (export "depMutI64" (global $dep_mut_i64))
  (export "depF32" (global $dep_f32))
  (export "depMutF32" (global $dep_mut_f32))
  (export "depF64" (global $dep_f64))
  (export "depMutF64" (global $dep_mut_f64))
  (export "depExternref" (global $dep_externref))
  (export "depMutExternref" (global $dep_mut_externref))
  (export "depV128" (global $dep_v128))
  (export "depMutV128" (global $dep_mut_v128))
  (export "\u{1f680}localI32" (global $local_i32))
  (export "localMutI32" (global $local_mut_i32))
  (export "localI64" (global $local_i64))
  (export "localMutI64" (global $local_mut_i64))
  (export "localF32" (global $local_f32))
  (export "localMutF32" (global $local_mut_f32))
  (export "localF64" (global $local_f64))
  (export "localMutF64" (global $local_mut_f64))
  (export "localExternref" (global $local_externref))
  (export "localMutExternref" (global $local_mut_externref))
  (export "localV128" (global $local_v128))
  (export "localMutV128" (global $local_mut_v128))
  (export "getImportedMutI32" (func $getImportedMutI32))
  (export "getImportedMutI64" (func $getImportedMutI64))
  (export "getImportedMutF32" (func $getImportedMutF32))
  (export "getImportedMutF64" (func $getImportedMutF64))
  (export "getImportedMutExternref" (func $getImportedMutExternref))
  (export "getLocalMutI32" (func $getLocalMutI32))
  (export "getLocalMutI64" (func $getLocalMutI64))
  (export "getLocalMutF32" (func $getLocalMutF32))
  (export "getLocalMutF64" (func $getLocalMutF64))
  (export "getLocalMutExternref" (func $getLocalMutExternref))
  (export "getLocalMutV128" (func $getLocalMutV128))
  (export "getDepMutI32" (func $getDepMutI32))
  (export "getDepMutI64" (func $getDepMutI64))
  (export "getDepMutF32" (func $getDepMutF32))
  (export "getDepMutF64" (func $getDepMutF64))
  (export "getDepMutExternref" (func $getDepMutExternref))
  (export "getDepMutV128" (func $getDepMutV128))
  (export "setImportedMutI32" (func $setImportedMutI32))
  (export "setImportedMutI64" (func $setImportedMutI64))
  (export "setImportedMutF32" (func $setImportedMutF32))
  (export "setImportedMutF64" (func $setImportedMutF64))
  (export "setImportedMutExternref" (func $setImportedMutExternref))
  (export "setLocalMutI32" (func $setLocalMutI32))
  (export "setLocalMutI64" (func $setLocalMutI64))
  (export "setLocalMutF32" (func $setLocalMutF32))
  (export "setLocalMutF64" (func $setLocalMutF64))
  (export "setLocalMutExternref" (func $setLocalMutExternref))
  (export "setLocalMutV128" (func $setLocalMutV128))
  (export "setDepMutI32" (func $setDepMutI32))
  (export "setDepMutI64" (func $setDepMutI64))
  (export "setDepMutF32" (func $setDepMutF32))
  (export "setDepMutF64" (func $setDepMutF64))
  (export "setDepMutExternref" (func $setDepMutExternref))
  (export "setDepMutV128" (func $setDepMutV128))
  (func $getImportedMutI32 (;0;) (type 0) (result i32)
    global.get $imported_mut_i32
  )
  (func $getImportedMutI64 (;1;) (type 1) (result i64)
    global.get $imported_mut_i64
  )
  (func $getImportedMutF32 (;2;) (type 2) (result f32)
    global.get $imported_mut_f32
  )
  (func $getImportedMutF64 (;3;) (type 3) (result f64)
    global.get $imported_mut_f64
  )
  (func $getImportedMutExternref (;4;) (type 4) (result externref)
    global.get $imported_mut_externref
  )
  (func $setImportedMutI32 (;5;) (type 6) (param i32)
    local.get 0
    global.set $imported_mut_i32
  )
  (func $setImportedMutI64 (;6;) (type 7) (param i64)
    local.get 0
    global.set $imported_mut_i64
  )
  (func $setImportedMutF32 (;7;) (type 8) (param f32)
    local.get 0
    global.set $imported_mut_f32
  )
  (func $setImportedMutF64 (;8;) (type 9) (param f64)
    local.get 0
    global.set $imported_mut_f64
  )
  (func $setImportedMutExternref (;9;) (type 10) (param externref)
    local.get 0
    global.set $imported_mut_externref
  )
  (func $getLocalMutI32 (;10;) (type 0) (result i32)
    global.get $local_mut_i32
  )
  (func $getLocalMutI64 (;11;) (type 1) (result i64)
    global.get $local_mut_i64
  )
  (func $getLocalMutF32 (;12;) (type 2) (result f32)
    global.get $local_mut_f32
  )
  (func $getLocalMutF64 (;13;) (type 3) (result f64)
    global.get $local_mut_f64
  )
  (func $getLocalMutExternref (;14;) (type 4) (result externref)
    global.get $local_mut_externref
  )
  (func $getLocalMutV128 (;15;) (type 5) (result v128)
    global.get $local_mut_v128
  )
  (func $setLocalMutI32 (;16;) (type 6) (param i32)
    local.get 0
    global.set $local_mut_i32
  )
  (func $setLocalMutI64 (;17;) (type 7) (param i64)
    local.get 0
    global.set $local_mut_i64
  )
  (func $setLocalMutF32 (;18;) (type 8) (param f32)
    local.get 0
    global.set $local_mut_f32
  )
  (func $setLocalMutF64 (;19;) (type 9) (param f64)
    local.get 0
    global.set $local_mut_f64
  )
  (func $setLocalMutExternref (;20;) (type 10) (param externref)
    local.get 0
    global.set $local_mut_externref
  )
  (func $setLocalMutV128 (;21;) (type 11) (param v128)
    local.get 0
    global.set $local_mut_v128
  )
  (func $getDepMutI32 (;22;) (type 0) (result i32)
    global.get $dep_mut_i32
  )
  (func $getDepMutI64 (;23;) (type 1) (result i64)
    global.get $dep_mut_i64
  )
  (func $getDepMutF32 (;24;) (type 2) (result f32)
    global.get $dep_mut_f32
  )
  (func $getDepMutF64 (;25;) (type 3) (result f64)
    global.get $dep_mut_f64
  )
  (func $getDepMutExternref (;26;) (type 4) (result externref)
    global.get $dep_mut_externref
  )
  (func $getDepMutV128 (;27;) (type 5) (result v128)
    global.get $dep_mut_v128
  )
  (func $setDepMutI32 (;28;) (type 6) (param i32)
    local.get 0
    global.set $dep_mut_i32
  )
  (func $setDepMutI64 (;29;) (type 7) (param i64)
    local.get 0
    global.set $dep_mut_i64
  )
  (func $setDepMutF32 (;30;) (type 8) (param f32)
    local.get 0
    global.set $dep_mut_f32
  )
  (func $setDepMutF64 (;31;) (type 9) (param f64)
    local.get 0
    global.set $dep_mut_f64
  )
  (func $setDepMutExternref (;32;) (type 10) (param externref)
    local.get 0
    global.set $dep_mut_externref
  )
  (func $setDepMutV128 (;33;) (type 11) (param v128)
    local.get 0
    global.set $dep_mut_v128
  )
)
