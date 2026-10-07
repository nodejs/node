// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
let tag = builder.addTag(kSig_v_v);

// Not inlinable, and emitted after the block below becomes reachable again.
let imported = builder.addImport("m", "f", kSig_v_v);

let dummy = builder.addFunction("dummy", sig_v_v).addBody([]).exportFunc();

// Never returns, so after inlining its return block has no predecessors and the
// assembler keeps generating unreachable operations, while the wasm decoder
// still considers the following code reachable.
let never_returns =
    builder.addFunction("never_returns", sig_v_v).addBody([kExprUnreachable]);

let get_cont =
    builder.addFunction("get_cont", makeSig([], [wasmRefType(cont_v_v)]))
        .addBody([
          kExprRefFunc, dummy.index,
          kExprContNew, cont_v_v,
        ]);

builder.addFunction("main", kSig_v_i)
    .addBody([
      kExprBlock, kWasmVoid,
        // Gives the end of the block a reachable predecessor.
        kExprLocalGet, 0,
        kExprBrIf, 0,
        kExprCallFunction, never_returns.index,
        kExprBlock, kWasmRefNull, cont_v_v,
          kExprCallFunction, get_cont.index,
          kExprResume, cont_v_v, 1, kOnSuspend, tag, 0,
          kExprRefNull, cont_v_v,
        kExprEnd,
        kExprDrop,
      kExprEnd,
      kExprCallFunction, imported,
    ])
    .exportFunc();

let instance = builder.instantiate({m: {f: () => {}}});
instance.exports.main(1);
