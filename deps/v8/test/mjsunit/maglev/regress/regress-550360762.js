// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --maglev-inlining --allow-natives-syntax

const probe = {a: 1};

function Receiver(v) {
  this.p0 = 10;
  this.p1 = 11;
  this.p2 = 12;
  this.p3 = 13;
  this.x = v;
  this.y = 99;
  for (let i = 0; i < 4; ++i) Object.defineProperty(this, 'p' + i, {enumerable: false});
}

function erasePressureThenRevalidate(victim, other) {
  delete victim.x;
  let key;
  OUT: {
    for (key in other) break OUT;
    throw 0;
  }
  return other.hasOwnProperty(key);
}

function vulnerable(victim, other) {
  for (let key in victim) {
    erasePressureThenRevalidate(victim, other);
    return victim[key];
  }
  return -1;
}

%PrepareFunctionForOptimization(Receiver);
%PrepareFunctionForOptimization(erasePressureThenRevalidate);
%PrepareFunctionForOptimization(vulnerable);

for (let i = 0; i < 2; ++i) {
  vulnerable(new Receiver(0), probe);
}
%OptimizeMaglevOnNextCall(vulnerable);
vulnerable(new Receiver(0), probe);

const victim = new Receiver(1337);
assertEquals(undefined, vulnerable(victim, probe));
