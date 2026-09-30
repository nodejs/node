// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --sandbox-testing

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// The DrumBrake interpreter reaches its InterpreterHandle through the in-cage
// Tuple2 stored in trusted_data->interpreter_object() (map@0, value1@4 =
// WasmInstanceObject, value2@8 = Managed<InterpreterHandle>). A sandbox
// attacker can overwrite value2 with another instance's handle -- it carries
// the same external-pointer tag, so the cast succeeds -- which runs that
// instance's bytecode while Runtime_WasmRunInterpreter marshals the results
// using the victim's (mismatched) signature. Here the confused function returns
// an attacker-controlled i64 that is then marshalled as an externref, giving an
// out-of-cage read. This must be detected as a sandbox violation and terminate
// the process rather than dereferencing the attacker-controlled address.

// Instance A returns an externref, so its result is marshalled as a reference.
// Instance B takes an i64 and returns it unchanged.
function makeInstance(params, returns, body) {
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', makeSig(params, returns))
      .addBody(body)
      .exportFunc();
  return builder.instantiate();
}

const instanceA =
    makeInstance([kWasmI64], [kWasmExternRef], [kExprRefNull, kExternRefCode]);
const instanceB = makeInstance([kWasmI64], [kWasmI64], [kExprLocalGet, 0]);

// Trigger interpretation once so each instance's interpreter Tuple2 exists.
instanceA.exports.main(0n);
instanceB.exports.main(0n);

// Sandbox memory-corruption helpers.
const kHeapObjectTag = 1;
const kValue2Offset = 8;  // Tuple2::value2 (Managed<InterpreterHandle>).
const kScanSize = 0x10000;

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
function read32(addr) {
  return memory.getUint32(addr, true);
}
function write32(addr, value) {
  memory.setUint32(addr, value, true);
}

// Each instance's interpreter Tuple2 (map@0, value1@4 = WasmInstanceObject,
// value2@8 = Managed<InterpreterHandle>) is only referenced from out-of-cage
// trusted data, so locate it by scanning the cage for a Tuple2 whose value1
// field (a tagged pointer) points at the instance. All Tuple2s share the same
// map, so cross-check both instances' candidates by identical map value; this
// disambiguates unrelated objects without hardcoding the build-specific map.
function findInterpreterTuples(start, taggedA, taggedB) {
  const bByMap = new Map();
  for (let b = start; b < start + kScanSize; b += 4) {
    if (read32(b + 4) === taggedB) bByMap.set(read32(b), b);
  }
  for (let a = start; a < start + kScanSize; a += 4) {
    if (read32(a + 4) !== taggedA) continue;
    const map = read32(a);
    if (bByMap.has(map)) return [a, bByMap.get(map)];
  }
  return [0, 0];
}

const addrA = Sandbox.getAddressOf(instanceA);
const addrB = Sandbox.getAddressOf(instanceB);
const start = Math.min(addrA, addrB);
const taggedA = (addrA + kHeapObjectTag) >>> 0;
const taggedB = (addrB + kHeapObjectTag) >>> 0;
const [tupleA, tupleB] = findInterpreterTuples(start, taggedA, taggedB);
assertTrue(tupleA !== 0 && tupleB !== 0, 'could not locate interpreter Tuple2');

// Swap instance A's interpreter handle for instance B's.
write32(tupleA + kValue2Offset, read32(tupleB + kValue2Offset));

// A now runs B's body: the i64 operand is returned and then marshalled as an
// externref, dereferencing an attacker-controlled out-of-cage address.
instanceA.exports.main(0x414141414141n);

assertUnreachable('Process should have been killed.');
