// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax --expose-gc --mock-arraybuffer-allocator

let gCounter = 1000;
let gMinHeap = new ArrayBuffer(1 << 12);
let gStdlib = {Uint8Array: Uint8Array};

// The template of asm.js modules used in this test.
function Template(stdlib, ffi, heap) {
  "use asm";
  var MEM8 = new stdlib.Uint8Array(heap);
  function foo() { return VAL; }
  return { foo: foo };
}

// Create a fresh module each time.
function NewModule() {
  // Use eval() to get a unique module each time.
  let val = gCounter++;
  let string = (Template + "; Template").replace("VAL", "" + val);
//  print(string);
  let module = eval(string);
//  print(module);
  module(gStdlib, {}, gMinHeap);
  assertTrue(%IsAsmWasmCode(module));
  return {module: module, val: val};
}

(function TestValid_PowerOfTwo() {
  print("TestValid_PowerOfTwo...");
  let r = NewModule();
  for (let i = 12; i <= 24; i++) {
    gc();  // Likely OOM otherwise.
    let size = 1 << i;
    print("  size=" + size);
    let heap = new ArrayBuffer(size);
    var instance = r.module(gStdlib, {}, heap);
    assertTrue(%IsAsmWasmCode(r.module));
    assertEquals(r.val, instance.foo());
  }
})();

(function TestValid_Multiple() {
  print("TestValid_Multiple...");
  let r = NewModule();
  for (let i = 1; i < 47; i += 7) {
    gc();  // Likely OOM otherwise.
    let size = i * (1 << 24);
    print("  size=" + size);
    let heap = new ArrayBuffer(size);
    var instance = r.module(gStdlib, {}, heap);
    assertTrue(%IsAsmWasmCode(r.module));
    assertEquals(r.val, instance.foo());
  }
})();

(function TestInvalid_TooSmall() {
  print("TestInvalid_TooSmall...");
  for (let i = 1; i < 12; i++) {
    let size = 1 << i;
    print("  size=" + size);
    let r = NewModule();
    let heap = new ArrayBuffer(size);
    var instance = r.module(gStdlib, {}, heap);
    assertFalse(%IsAsmWasmCode(r.module));
    assertEquals(r.val, instance.foo());
  }
})();

(function TestInValid_NonPowerOfTwo() {
  print("TestInvalid_NonPowerOfTwo...");
  for (let i = 12; i <= 24; i++) {
    gc();  // Likely OOM otherwise.
    let size = 1 + (1 << i);
    print("  size=" + size);
    let r = NewModule();
    let heap = new ArrayBuffer(size);
    var instance = r.module(gStdlib, {}, heap);
    assertFalse(%IsAsmWasmCode(r.module));
    assertEquals(r.val, instance.foo());
  }
})();

(function TestInValid_NonMultiple() {
  print("TestInvalid_NonMultiple...");
  for (let i = (1 << 24); i < (1 << 25); i += (1 << 22)) {
    gc();  // Likely OOM otherwise.
    let size = i + (1 << 20);
    print("  size=" + size);
    let r = NewModule();
    let heap = new ArrayBuffer(size);
    var instance = r.module(gStdlib, {}, heap);
    assertFalse(%IsAsmWasmCode(r.module));
    assertEquals(r.val, instance.foo());
  }
})();
