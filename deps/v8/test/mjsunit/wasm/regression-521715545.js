// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage --enable-inspector

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const func = builder.addFunction('main', kSig_v_v)
    .addBody([
        kExprI32Const, 0,
        kExprI32Const, 1,
        kExprI32Add,
        kExprDrop
    ])
    .exportFunc();
const buffer = builder.toBuffer();

let scriptId;
globalThis.receive = function(msg) {
    let response = JSON.parse(msg);
    if (response.method === 'Debugger.scriptParsed') {
        scriptId = response.params.scriptId;
    }
};

send(JSON.stringify({
    id: 1,
    method: 'Debugger.enable'
}));

const module = new WebAssembly.Module(buffer);
const instance = new WebAssembly.Instance(module);

// Offset into the code section for the 'i32.add' instruction.
// The header is 8 bytes, sections are typically 1 byte ID + length.
// WasmModuleBuilder offsets are relative to the start of the buffer.
const offset = func.body_offset + 2;

send(JSON.stringify({
    id: 2,
    method: 'Debugger.setBreakpoint',
    params: {
        location: {
            scriptId: scriptId,
            lineNumber: 0,
            columnNumber: offset
        },
    }
}));

// If we reached here without crashing, the fix works.
