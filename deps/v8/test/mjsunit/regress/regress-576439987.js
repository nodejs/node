// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --maglev

const prototypes = Array.from({length: 50}, (_, i) => ({['p' + i]: i}));
const empty = new Uint8Array(0);
const replacement = new Uint8Array([77]);

// Uses DefineNamedOwnPropertyInLiteral for writing the value.
function make(prototype, value) { return {__proto__: prototype, value}; }

// Evil base class; the ctor always returns the object you passed in instead of
// a new instance of Base.
class Base { constructor(object) { return object; } }

let nextValue = empty;

// Uses DefineNamedOwnProperty for writing the value. Because of the base
// class, the object we're writing the value in might or might not already have
// a property called "value".
class Define extends Base { value = nextValue; }

// Make both the DefineNamedOwnPropertyInLiteral and the DefineNamedOwnProperty
// call sites megamorphic.
for (let i = 0; i < 50; ++i) {
  make(prototypes[i % prototypes.length], empty);
  new Define({['q' + i]: i});
}

const victim = make(prototypes[0], empty);
function overwrite(value) {
  nextValue = value;
  make(prototypes[0], empty);  // Cache the literal's initializing handler.
  new Define(victim);          // Reuse it for a non-initializing store.
}

function read() { return victim.value[0]; }
%PrepareFunctionForOptimization(read);
read();

const buffer = new ArrayBuffer(64 * 1024 * 1024);
let view = new Uint8Array(buffer);
view[0] = 133;
overwrite(view);

%OptimizeMaglevOnNextCall(read);
read();
assertMaglevved(read);

overwrite(replacement);
view = null;
assertEquals(77, victim.value[0]);
assertEquals(77, read());
assertMaglevved(read);

gc();
buffer.transfer(0);
assertEquals(77, read());

assertMaglevved(read);
