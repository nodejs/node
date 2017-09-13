// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-threads

function assertMemoryIsValid(memory) {
  assertSame(WebAssembly.Memory.prototype, memory.__proto__);
  assertSame(WebAssembly.Memory, memory.constructor);
  assertTrue(memory instanceof Object);
  assertTrue(memory instanceof WebAssembly.Memory);
}

(function TestConstructorWithShared() {
  print("TestConstructorWithShared");
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: true});
  assertMemoryIsValid(memory);
  // Assert that the buffer is frozen when memory is shared.
  assertTrue(Object.isFrozen(memory.buffer));
})();

(function TestConstructorWithUndefinedShared() {
  print("TestConstructorWithUndefinedShared");
  // Maximum = undefined, shared = undefined.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: undefined, shared: undefined});
  assertMemoryIsValid(memory);
})();

(function TestConstructorWithNumericShared() {
  print("TestConstructorWithNumericShared");
  // For numeric values, shared = true.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: 10, shared: 2098665});
  assertMemoryIsValid(memory);
})();

(function TestConstructorWithEmptyStringShared() {
  print("TestConstructorWithEmptyStringShared");
  // Maximum = undefined, shared = false.
  let memory = new WebAssembly.Memory({
    initial: 0, maximum: undefined, shared: ""});
  assertMemoryIsValid(memory);
})();

(function TestConstructorWithUndefinedMaxShared() {
  print("TestConstructorWithUndefinedMaxShared");
  // New memory with Maximum = undefined, shared = true => TypeError.
  assertThrows(() => new WebAssembly.Memory({initial: 0, shared: true}),
      TypeError);
})();
