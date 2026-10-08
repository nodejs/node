// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --stress-wasm-stack-switching

load('test/mjsunit/wasm/wasm-module-builder.js');

let scriptId;
let bodyOffset;

globalThis.receive = function (message) {
  let json = JSON.parse(message);
  if (json.params && json.params.scriptId) {
    scriptId = json.params.scriptId;
  }
};

const builder = new WasmModuleBuilder();
builder.addMemory(1);
const tag = builder.addTag(kSig_v_v);

// target2: throws and catches internally.
// With --stress-wasm-stack-switching, this will run on Stack 2.
builder.addFunction('target2', kSig_v_v)
    .addBody([
      kExprTry, kWasmVoid,
        kExprThrow, tag,
      kExprCatch, tag,
      kExprEnd
    ]).exportFunc();

// target: hits breakpoint.
// With --stress-wasm-stack-switching, this will run on Stack 1.
const body = [...wasmI32Const(5), kExprI32LoadMem, 0, kExprDrop,
              ...wasmI32Const(5), ...wasmI32Const(5), 0];
const target = builder.addFunction('target', kSig_v_v)
                      .addBody(body).exportFunc();

const buffer = builder.toBuffer();
// Offset of the I32LoadMem instruction.
bodyOffset = target.body_offset + 6;

send(JSON.stringify({
  id: 1,
  method: 'Debugger.enable'
}));

// Instantiate triggers compilation and sends scriptParsed event (which sets
// scriptId).
const instance = new WebAssembly.Instance(new WebAssembly.Module(buffer));

// Condition for breakpoint: call target2.
const condition = `(function() {
  instance.exports.target2();
})()`;

send(JSON.stringify({
  id: 2,
  method: 'Debugger.setBreakpoint',
  params: {
    location: {
      scriptId: scriptId,
      lineNumber: 0,
      columnNumber: bodyOffset
    },
    condition: condition
  }
}));

// Run target. It will hit breakpoint, evaluate condition (runs target2 on Stack
// 2), target2 catches exception and clears secondary SP.  When target resumes,
// it should not crash.
assertThrows(() => instance.exports.target(),
             WebAssembly.RuntimeError, /unreachable/);
