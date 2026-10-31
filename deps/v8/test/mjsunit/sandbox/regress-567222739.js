// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --sparkplug --sparkplug-plus

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const field = (type, name) =>
    Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor(type), name);
const read32 = p => memory.getUint32(p, true);
const write32 = (p, value) => memory.setUint32(p, value, true);

const cell_offset = field('JS_FUNCTION_TYPE', 'feedback_cell');
const value_offset = field('FEEDBACK_CELL_TYPE', 'value');
const data_offset = field('FEEDBACK_VECTOR_TYPE', 'data');
const get_vector = fn =>
    read32(read32(Sandbox.getAddressOf(fn) + cell_offset) - 1 + value_offset) -
    1;

function test_corrupted_offset(handler_smi) {
  const fn = eval(`(function(o) { return o.value; })`);
  const object = {value: 42};
  %CompileBaseline(fn);
  %PrepareFunctionForOptimization(fn);

  const slot = get_vector(fn) + data_offset;
  write32(slot, read32(Sandbox.getAddressOf(object)) | 2);
  write32(slot + 4, handler_smi);

  assertEquals(42, fn(object));
}

// Forged Smi LoadHandler: non-double in-object field at word offset 0.
test_corrupted_offset(2 * ((1 << 17) | 5));

// Forged Smi LoadHandler: non-double property-array field at word offset 0.
test_corrupted_offset(2 * 5);
