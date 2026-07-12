// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax  --stress-incremental-marking

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

gc({type: "major"});
gc({type: "major"});

const builder = new WasmModuleBuilder();

let $trigger_gc = builder.addImport("js", "func", kSig_v_i);
let $trigger_gc_signature = builder.addType(kSig_v_i);
builder.addDeclarativeElementSegment([$trigger_gc]);

let f = builder.addFunction("opt_me", kSig_v_i) // local0: function parameter, represents the loop count cnt
.addLocals(kWasmI32, 1) // local1: loop counter i
.addLocals(wasmRefType(kSig_v_i), 1) // local2: used to save the reference to function $trigger_gc
.addBody([
    // Generate function reference
    // ref.func will allocate `WasmInternalFunction` objects in TrustedSpace, allocating multiple objects that lack references
    kExprRefFunc, $trigger_gc,
    kExprLocalSet, 2,
    kExprRefFunc, $trigger_gc,
    kExprLocalSet, 2,
    kExprRefFunc, $trigger_gc,
    kExprLocalSet, 2,

    // This one keeps a reference. When GC occurs, the previous `WasmInternalFunction` objects will be collected,
    // so this object will be migrated to a lower address region.
    kExprRefFunc, $trigger_gc,
    kExprLocalSet, 2,

    kExprLoop, kWasmVoid,   // while(True)
        kExprLocalGet, 1,   // if( i < cnt )
        kExprLocalGet, 0,
        kExprI32LtS,
        kExprIf, kWasmVoid,

            kExprLocalGet, 1,   // $trigger_gc(i)
            kExprLocalGet, 2,
            kExprCallRef, $trigger_gc_signature,

            kExprLocalGet, 1,   // i++
            kExprI32Const, 1,
            kExprI32Add,
            kExprLocalSet, 1,

            kExprBr, 1,
        kExprEnd,
    kExprEnd,

]).exportFunc();

function trigger_gc(i) {
    if(i==50) {
        gc({type: "major"});
    }
}

let imports = {
    js: {
        func: trigger_gc
    }
}
const wasmInstance = builder.instantiate(imports);
let opt_me = wasmInstance.exports.opt_me;
%WasmTierUpFunction(opt_me);
opt_me(100);

%DebugPrint(opt_me);
