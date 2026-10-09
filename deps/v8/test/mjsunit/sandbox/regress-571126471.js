// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const offsetOf = (object, field) =>
    Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdOf(object), field);
const jsFuncSfiOffset = offsetOf(function() {}, 'shared_function_info');

// Create a lazy async function with an inner skippable function so V8 attaches
// an on-heap PreparseData object in the sandbox.
const bodyPadding = ' '.repeat(2 ** 11);
const origSource =
    `async function victim() { let x = 1; function skip_me() { return x; } ${bodyPadding} }`;
(0, eval)(origSource);

const target = globalThis.victim;
const targetAddr = Sandbox.getAddressOf(target);
const sfiAddr = memory.getUint32(targetAddr + jsFuncSfiOffset, true) - 1;
const sfi = Sandbox.getObjectAt(sfiAddr);
const script = Sandbox.dereferenceTaggedPointerField(sfi, 'script');

// PreparseData is allocated immediately after victim's SharedFunctionInfo.
const preparseDataAddr = sfiAddr + 0x30;

const prefix = 'async function victim() { /* ';
const commentCode = '}) ) => { await 0; })(); } */ ';
const awaits = Array.from({length: 64}, () => 'await 1').join(' + ');
const middle = `(async (a = ${awaits}, (b = function skip_me`;
const suffix = '() {})) => {})() }';

const targetBracePos = prefix.length;
const skipMeStartPos = (prefix + commentCode + middle).length;
const newSource =
    (prefix + commentCode + middle + suffix).padEnd(origSource.length, ' ');

function encodeVarint32(val) {
  const out = [];
  do {
    let b = val & 0x7f;
    val >>>= 7;
    if (val) b |= 0x80;
    out.push(b);
  } while (val);
  return out;
}

const newPreparseBytes = [
  ...encodeVarint32(skipMeStartPos),
  ...encodeVarint32(targetBracePos + 1),
  ...encodeVarint32(2),
  ...encodeVarint32(0),
  0, 0, 0, 0, 0,
];

memory.setInt32(preparseDataAddr + 4, newPreparseBytes.length, true);
for (let i = 0; i < newPreparseBytes.length; i++) {
  memory.setUint8(preparseDataAddr + 12 + i, newPreparseBytes[i]);
}

const scriptPosition = offsetOf(script, 'wasm_managed_native_module');
const scriptSourceOffset = scriptPosition - 36;
const tagged = obj => (Sandbox.getAddressOf(obj) + 1) >>> 0;
Sandbox.corruptObjectField(script, scriptSourceOffset, tagged(newSource), 32);

target();
