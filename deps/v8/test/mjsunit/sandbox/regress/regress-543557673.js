// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax

const moduleABytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x16,                    // section length 22
  0x05,                    // types count 5
  0x60,                    // type #0 $type0 kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #1 $type1 kind: func
  0x01, 0x6f,              // param count 1:  externref
  0x00,                    // return count 0
  0x60,                    // type #2 $type2 kind: func
  0x00,                    // param count 0
  0x01, 0x6f,              // return count 1:  externref
  0x60,                    // type #3 $type3 kind: func
  0x01, 0x7f,              // param count 1:  i32
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #4 $type4 kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x02,                    // section kind: Import
  0x1b,                    // section length 27
  0x02,                    // imports count 2
  0x01,                    // module name length:  1
  0x6d,                    // module name: m
  0x09,                    // field name length:  9
  0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e,
  0x73,                    // field name: functions
  0x01, 0x70, 0x01,        // kind or compact flag: table funcref with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1
                           // import #0
  0x01,                    // module name length:  1
  0x6d,                    // module name: m
  0x03,                    // field name length:  3
  0x74, 0x61, 0x67,        // field name: tag
  0x04, 0x00, 0x01,        // kind or compact flag: tag (param externref)
                           // import #1

  0x03,                    // section kind: Function
  0x03,                    // section length 3
  0x02,                    // functions count 2
  0x03,                    // 0 $driver (param i32) (result i32)
  0x04,                    // 1 $seed (result i32)

  0x04,                    // section kind: Table
  0x05,                    // section length 5
  0x01, 0x69, 0x01,        // table count 1:  exnref with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1

  0x06,                    // section kind: Global
  0x20,                    // section length 32
  0x06,                    // globals count 6
  0x7f, 0x01,              // global #0: i32 mutable
  0x41, 0xef, 0x00, 0x0b,  // i32.const 111
  0x7f, 0x01,              // global #1: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0
  0x7f, 0x01,              // global #2: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0
  0x7f, 0x01,              // global #3: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0
  0x7f, 0x01,              // global #4: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0
  0x7f, 0x01,              // global #5: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0

  0x07,                    // section kind: Export
  0x1e,                    // section length 30
  0x03,                    // exports count 3
                           // export # 0
  0x0a,                    // field name length:  10
  0x65, 0x78, 0x63, 0x65, 0x70, 0x74, 0x69, 0x6f,
  0x6e, 0x73,              // field name: exceptions
  0x01, 0x01,              // kind: table index:  1
                           // export # 1
  0x06,                    // field name length:  6
  0x64, 0x72, 0x69, 0x76, 0x65, 0x72,  // field name: driver
  0x00, 0x00,              // kind: function index:  0
                           // export # 2
  0x04,                    // field name length:  4
  0x73, 0x65, 0x65, 0x64,  // field name: seed
  0x00, 0x01,              // kind: function index:  1

  0x0a,                    // section kind: Code
  0x30,                    // section length 48
  0x02,                    // functions count 2
                           // function #0 $driver
  0x29,                    // body size 41
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x11, 0x00, 0x00,        // call_indirect (result i32)
  0x1a,                    // drop
  0x20, 0x00,              // local.get $var0
  0x04, 0x40,              // if
  0x02, 0x02,              // block (result externref) $label0
  0x1f, 0x02, 0x01, 0x00, 0x00, 0x00,  // try_table (result externref) catch $m.tag $label0
  0x41, 0x00,              // i32.const 0
  0x25, 0x01,              // table.get $exceptions
  0x0a,                    // throw_ref
  0x0b,                    // end $label0
  0x0b,                    // end
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x24, 0x05,              // global.set $global5
  0x41, 0x00,              // i32.const 0
  0x11, 0x00, 0x00,        // call_indirect (result i32)
  0x0f,                    // return
  0x0b,                    // end
  0x41, 0x00,              // i32.const 0
  0x0b,                    // end
                           // function #1 $seed
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x41, 0x07,              // i32.const 7
  0x0b,                    // end

  0x00,                    // section kind: Unknown
  0x16,                    // section length 22
  0x04,                    // section name length:  4
  0x6e, 0x61, 0x6d, 0x65,  // section name: name
  0x01,                    // name type: function
  0x0f,                    // payload length: 15
  0x02,                    // names count 2
  0x00, 0x06,              // index 0 name length:  6
  0x64, 0x72, 0x69, 0x76, 0x65, 0x72,  // name: driver
  0x01, 0x04,              // index 1 name length:  4
  0x73, 0x65, 0x65, 0x64,  // name: seed
]);
const moduleDBytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x0d,                    // section length 13
  0x03,                    // types count 3
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x04,                    // section length 4
  0x03,                    // functions count 3
  0x00,                    // 0 $pad0 (result i32)
  0x01,                    // 1 $pad1 (result i32)
  0x02,                    // 2 $target (result i32)

  0x07,                    // section kind: Export
  0x0a,                    // section length 10
  0x01,                    // exports count 1: export # 0
  0x06,                    // field name length:  6
  0x74, 0x61, 0x72, 0x67, 0x65, 0x74,  // field name: target
  0x00, 0x02,              // kind: function index:  2

  0x0a,                    // section kind: Code
  0x11,                    // section length 17
  0x03,                    // functions count 3
                           // function #0 $pad0
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x0b,                    // end
                           // function #1 $pad1
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x41, 0x01,              // i32.const 1
  0x0b,                    // end
                           // function #2 $target
  0x05,                    // body size 5
  0x00,                    // 0 entries in locals list
  0x41, 0xde, 0x01,        // i32.const 222
  0x0b,                    // end

  0x00,                    // section kind: Unknown
  0x1c,                    // section length 28
  0x04,                    // section name length:  4
  0x6e, 0x61, 0x6d, 0x65,  // section name: name
  0x01,                    // name type: function
  0x15,                    // payload length: 21
  0x03,                    // names count 3
  0x00, 0x04,              // index 0 name length:  4
  0x70, 0x61, 0x64, 0x30,  // name: pad0
  0x01, 0x04,              // index 1 name length:  4
  0x70, 0x61, 0x64, 0x31,  // name: pad1
  0x02, 0x06,              // index 2 name length:  6
  0x74, 0x61, 0x72, 0x67, 0x65, 0x74,  // name: target
]);

const kHeapObjectTag = 1;
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const read32 = address => memory.getUint32(address, true);
const write32 = (address, val) => memory.setUint32(address, val >>> 0, true);
const tagged = object => (Sandbox.getAddressOf(object) + kHeapObjectTag) >>> 0;

function instanceTypeAt(address) {
  try {
    return Sandbox.getInstanceTypeOfObjectAt(address);
  } catch (_) {
    return undefined;
  }
}

function descriptorArrayOf(object) {
  const map = read32(Sandbox.getAddressOf(object)) & ~kHeapObjectTag;
  const size = Sandbox.getSizeOfObjectAt(map);
  for (let offset = 0; offset < size; offset += 4) {
    const candidate = read32(map + offset) & ~kHeapObjectTag;
    if (instanceTypeAt(candidate) === 'DESCRIPTOR_ARRAY_TYPE') return candidate;
  }
  throw new Error('descriptor array not found');
}

function findTaggedWord(objectAddress, expectedType, expectedValue) {
  const size = Sandbox.getSizeOfObjectAt(objectAddress);
  for (let offset = 0; offset < size; offset += 4) {
    const value = read32(objectAddress + offset);
    if (expectedValue !== undefined && value !== expectedValue) continue;
    if (instanceTypeAt(value & ~kHeapObjectTag) === expectedType) {
      return {offset, value};
    }
  }
  throw new Error(`tagged ${expectedType} word not found`);
}

const table =
    new WebAssembly.Table({element: 'anyfunc', initial: 1, maximum: 1});
const d = new WebAssembly.Instance(new WebAssembly.Module(moduleDBytes));
const a = new WebAssembly.Instance(new WebAssembly.Module(moduleABytes), {
  m: {functions: table, tag: WebAssembly.JSTag},
});
if (d.exports.target() !== 222) throw new Error('D target sanity failed');

// Recover the private exception-tag symbol from a genuine exception.
const genuine =
    new WebAssembly.Exception(new WebAssembly.Tag({parameters: []}), []);
const genuineDescriptors = descriptorArrayOf(genuine);
const hiddenTagSymbol = findTaggedWord(genuineDescriptors, 'SYMBOL_TYPE').value;

let getterCount = 0;
const forged = Object.defineProperty({}, 'x', {
  get() {
    ++getterCount;
    table.set(0, d.exports.target);
    return undefined;
  },
  configurable: true,
});

// Replace only the one-property donor descriptor's key. Its accessor details
// and AccessorPair remain unchanged.
const forgedDescriptors = descriptorArrayOf(forged);
const xName = Object.getOwnPropertyNames(forged)[0];
const descriptorKey = findTaggedWord(
    forgedDescriptors,
    Sandbox.getInstanceTypeOfObjectAt(Sandbox.getAddressOf(xName)),
    tagged(xName));
write32(forgedDescriptors + descriptorKey.offset, hiddenTagSymbol);

// Put the ordinary JSReceiver into the backing FixedArray of the exported
// Wasm-declared exnref table.
const tableType = Sandbox.getInstanceTypeIdFor('WASM_TABLE_OBJECT_TYPE');
const entriesOffset = Sandbox.getFieldOffset(tableType, 'entries');
const fixedArrayType = Sandbox.getInstanceTypeIdFor('FIXED_ARRAY_TYPE');
const fixedArrayDataOffset = Sandbox.getFieldOffset(fixedArrayType, 'data');
const exnTableAddress = Sandbox.getAddressOf(a.exports.exceptions);
const entries = read32(exnTableAddress + entriesOffset) & ~kHeapObjectTag;
write32(entries + fixedArrayDataOffset, tagged(forged));

// Only the first call_indirect site receives feedback and is inlined. The
// second site remains uninitialized and therefore generic.
table.set(0, a.exports.seed);
for (let i = 0; i < 20; ++i) {
  if (a.exports.driver(0) !== 0) throw new Error('training failed');
}
%WasmTierUpFunction(a.exports.driver);
if (a.exports.driver(0) !== 0) throw new Error('optimized training failed');

const result = a.exports.driver(1);
const tableChanged = table.get(0) === d.exports.target;
assertEquals(getterCount, 0);
assertFalse(tableChanged);
