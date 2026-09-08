// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Wasm frame that has caught an exception, then makes a call that reenters
// the interpreter, must still be able to rethrow: the reentry must not lose the
// frame's caught_exceptions_ (crrev.com/c/7925613 cleared it in the frame
// bookmark, leaking the handle and null-dereferencing on rethrow). Cover all
// three reentry boundaries: an imported call, call_indirect and call_ref.

load('test/mjsunit/wasm/wasm-module-builder.js');

let instance;
function reenter() { instance.exports.noop(); }  // reenter the interpreter

const builder = new WasmModuleBuilder();
const sig_v_v = builder.addType(kSig_v_v);
const except = builder.addTag(kSig_v_v);
const reenterImport = builder.addImport('m', 'reenter', kSig_v_v);
builder.addFunction('noop', kSig_v_v).addBody([]).exportFunc();

// A funcref table whose single entry is the reentering import, reached by
// call_indirect directly and by call_ref via table.get + ref.cast.
builder.setTableBounds(1);
builder.addActiveElementSegment(0, wasmI32Const(0), [reenterImport]);

// try { throw } catch { <reenter via imported call>; rethrow }
builder.addFunction('run_call', kSig_v_v)
  .addBody([
    kExprTry, kWasmVoid,
      kExprThrow, except,
    kExprCatch, except,
      kExprCallFunction, reenterImport,
      kExprRethrow, 0,
    kExprEnd,
  ]).exportFunc();

// try { throw } catch { <reenter via call_indirect>; rethrow }
builder.addFunction('run_call_indirect', kSig_v_v)
  .addBody([
    kExprTry, kWasmVoid,
      kExprThrow, except,
    kExprCatch, except,
      ...wasmI32Const(0),
      kExprCallIndirect, sig_v_v, kTableZero,
      kExprRethrow, 0,
    kExprEnd,
  ]).exportFunc();

// try { throw } catch { <reenter via call_ref>; rethrow }
builder.addFunction('run_call_ref', kSig_v_v)
  .addBody([
    kExprTry, kWasmVoid,
      kExprThrow, except,
    kExprCatch, except,
      ...wasmI32Const(0),
      kExprTableGet, kTableZero,
      kGCPrefix, kExprRefCast, sig_v_v,
      kExprCallRef, sig_v_v,
      kExprRethrow, 0,
    kExprEnd,
  ]).exportFunc();

instance = builder.instantiate({ m: { reenter: reenter } });
assertThrows(() => instance.exports.run_call(), WebAssembly.Exception);
assertThrows(() => instance.exports.run_call_indirect(), WebAssembly.Exception);
assertThrows(() => instance.exports.run_call_ref(), WebAssembly.Exception);
