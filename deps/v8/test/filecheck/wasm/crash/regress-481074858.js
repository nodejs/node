// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

function createWasmBinaryWithDualBrTables(tableSize1, tableSize2) {
  const bytes = [];

  function push_varuint(val) {
    while (val >= 0x80) {
      bytes.push((val & 0x7F) | 0x80);
      val >>>= 7;
    }
    bytes.push(val);
  }

  // Wasm header
  bytes.push(0x00, 0x61, 0x73, 0x6d);  // magic
  bytes.push(0x01, 0x00, 0x00, 0x00);  // version

  // Type section (section 1)
  const typeSection = [];
  typeSection.push(1);  // num types
  // Type 0: (i32, i32) -> i32
  typeSection.push(0x60);  // func type
  typeSection.push(2);     // num params
  typeSection.push(0x7f);  // i32
  typeSection.push(0x7f);  // i32
  typeSection.push(1);     // num results
  typeSection.push(0x7f);  // i32

  bytes.push(1);  // type section ID
  push_varuint(typeSection.length);
  bytes.push(...typeSection);

  // Function section (section 3)
  const funcSection = [];
  funcSection.push(1);  // num functions
  funcSection.push(0);  // type index

  bytes.push(3);  // function section ID
  push_varuint(funcSection.length);
  bytes.push(...funcSection);

  // Export section (section 7)
  const exportSection = [];
  exportSection.push(1);                       // num exports
  exportSection.push(4);                       // name length
  exportSection.push(0x74, 0x65, 0x73, 0x74);  // "test"
  exportSection.push(0);                       // export kind (function)
  exportSection.push(0);                       // function index

  bytes.push(7);  // export section ID
  push_varuint(exportSection.length);
  bytes.push(...exportSection);

  // Code section (section 10)
  const funcBody = [];

  funcBody.push(0);  // local count

  // Structure:
  // block $outer (result i32)          ; This block will have many predecessors
  //   local.get 1                       ; condition for if/else
  //   if
  //     i32.const 1                     ; push value before br_table
  //     local.get 0                     ; switch value
  //     br_table [0, 0, ... (tableSize1 times)]  ; all target $outer
  //   else
  //     i32.const 2                     ; push value before br_table
  //     local.get 0                     ; switch value
  //     br_table [0, 0, ... (tableSize2 times)]  ; all target $outer
  //   end
  //   i32.const 0                       ; fallthrough value
  // end

  // block $outer (result i32)
  funcBody.push(0x02, 0x7f);  // block returning i32

  // local.get 1 (condition)
  funcBody.push(0x20, 0x01);

  // if (void)
  funcBody.push(0x04, 0x40);

  // --- IF branch ---
  // i32.const 1 (value for br_table to pass to outer block)
  funcBody.push(0x41, 0x01);

  // local.get 0
  funcBody.push(0x20, 0x00);

  // br_table with tableSize1 entries
  funcBody.push(0x0e);

  // Encode table count
  let tempBytes = [];
  let val = tableSize1;
  while (val >= 0x80) {
    tempBytes.push((val & 0x7F) | 0x80);
    val >>>= 7;
  }
  tempBytes.push(val);
  funcBody.push(...tempBytes);

  // All entries point to label 1 (which is $outer, since we're inside if which
  // is label 0)
  for (let i = 0; i <= tableSize1; i++) {
    funcBody.push(1);  // target: label 1 ($outer)
  }

  // else
  funcBody.push(0x05);

  // --- ELSE branch ---
  // i32.const 2 (value for br_table to pass to outer block)
  funcBody.push(0x41, 0x02);

  // local.get 0
  funcBody.push(0x20, 0x00);

  // br_table with tableSize2 entries
  funcBody.push(0x0e);

  // Encode table count
  tempBytes = [];
  val = tableSize2;
  while (val >= 0x80) {
    tempBytes.push((val & 0x7F) | 0x80);
    val >>>= 7;
  }
  tempBytes.push(val);
  funcBody.push(...tempBytes);

  // All entries point to label 1 (which is $outer, since we're inside else
  // which is label 0)
  for (let i = 0; i <= tableSize2; i++) {
    funcBody.push(1);  // target: label 1 ($outer)
  }

  // end if
  funcBody.push(0x0b);

  // Fallthrough case - provide value for block
  funcBody.push(0x41, 0x00);  // i32.const 0

  // end block
  funcBody.push(0x0b);

  // end function
  funcBody.push(0x0b);

  // Build code section
  const codeSection = [];
  codeSection.push(1);  // num functions

  // Function body with size prefix
  tempBytes = [];
  val = funcBody.length;
  while (val >= 0x80) {
    tempBytes.push((val & 0x7F) | 0x80);
    val >>>= 7;
  }
  tempBytes.push(val);
  codeSection.push(...tempBytes);
  codeSection.push(...funcBody);

  bytes.push(10);  // code section ID
  tempBytes = [];
  val = codeSection.length;
  while (val >= 0x80) {
    tempBytes.push((val & 0x7F) | 0x80);
    val >>>= 7;
  }
  tempBytes.push(val);
  bytes.push(...tempBytes);
  bytes.push(...codeSection);

  return new Uint8Array(bytes);
}

// Test with sizes that together exceed uint16_t max (65535)
// Each br_table can have up to 65520 entries
// We'll use 33000 + 33000 = 66000 > 65535
const size1 = 33000;
const size2 = 33000;
const expectedPredecessors = size1 + 1 + size2 + 1;  // +1 for default case each

print('=== Wasm br_table phi overflow PoC ===');
print('br_table size 1: ' + size1);
print('br_table size 2: ' + size2);
print('Expected total predecessors: ' + expectedPredecessors);
print('uint16_t max: 65535');
print('Overflow expected: ' + (expectedPredecessors > 65535));

print('\nCreating Wasm module...');
const binary = createWasmBinaryWithDualBrTables(size1, size2);
print('Binary size: ' + binary.length + ' bytes');

print('\nCompiling with --no-liftoff to force Turboshaft...');
// CHECK: Compiling
const module = new WebAssembly.Module(binary);
print('Compilation succeeded');

print('\nInstantiating...');
const instance = new WebAssembly.Instance(module);
print('Instance created');

print('\nCalling test function...');

// Test both branches
const result1 = instance.exports.test(5, 1);  // if branch
const result2 = instance.exports.test(5, 0);  // else branch

// This should have failed either on lazy compilation or (if lazy compilation
// was disabled) on compilation above.
// CHECK: # Check failed: inputs.size() <= std::numeric_limits
// CHECK-NOT: Results:
print('Results: ' + result1 + ', ' + result2);
