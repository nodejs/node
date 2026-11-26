// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --no-wasm-in-js-inlining-body

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Collecting multiple results from a JS import runs arbitrary JavaScript
// (iterator getters). That must not leave the context register clobbered when
// the first result is converted to a Wasm type.

// The terminal 'done' getter is a bound builtin, without a JS wrapper, so that
// the builtin's register state is observed directly by the wrapper.
const numbers = new Array(1);
numbers[0] = 1.2345678912345;
const terminalResult = Object.defineProperty({}, 'done', {
  get: Array.prototype.join.bind(numbers)
});

let resultIndex = 0;
const results = {
  [Symbol.iterator]() { return this; },
  next() {
    if (resultIndex === 0) {
      resultIndex++;
      return {done: false, value: Symbol('invalid-i32')};
    }
    if (resultIndex === 1) {
      resultIndex++;
      return {done: false, value: 7};
    }
    return terminalResult;
  }
};

const builder = new WasmModuleBuilder();
const importIndex = builder.addImport('m', 'f', kSig_ii_v);
builder.addFunction('run', kSig_ii_v)
    .addBody([kExprCallFunction, importIndex])
    .exportFunc();

const instance = builder.instantiate({m: {f() { return results; }}});
assertThrows(() => instance.exports.run(), TypeError);
