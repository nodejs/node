// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// NormalizedMapCache was indexed by a hash of the prototype and bit_field2
// only. A JSFunction and a plain object with prototype Function.prototype
// hashed to the same entry although they are never
// EquivalentToForNormalization, so alternating normalizations at the two
// sites evicted each other and no two objects shared a dictionary map.
// Including the instance type in the mixed hash lets these two sites use
// different entries and reuse their normalized maps.

const fns = [];
const objs = [];
for (let i = 0; i < 20; i++) {
  const f = function() {};
  delete f.length;
  fns.push(f);
  const o = Object.create(Function.prototype);
  o.a = 1;
  delete o.a;
  objs.push(o);
}
assertFalse(%HasFastProperties(fns[0]));
assertFalse(%HasFastProperties(objs[0]));
for (let i = 1; i < fns.length; i++) {
  assertTrue(%HaveSameMap(fns[0], fns[i]));
  assertTrue(%HaveSameMap(objs[0], objs[i]));
}
