// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev-non-eager-inlining --expose-gc --allow-natives-syntax
// Flags: --max-maglev-eager-inlined-bytecode-size=0

class D {
  constructor(x) {
    this.x = x;
    this.p = 7;
    this.q = 9;
  }
}

// Map: MD1 where x is const SMI
let dOld = new D(1);

// Map: MD2 (after adding "tail")
// Map transition: MD1 --tail--> MD2
let dExtra = new D(1);
dExtra.tail = 1;

class C {
  constructor(z) {
    this.z = z;
    this.q = 11;
  }
}

// Map: MC1 where z is a const SMI
let cOld = new C(1);

// Map: MC2 w where z is a const HeapNumber
let cTarget = new C(1.5);

// Migrate to MC2: this makes MC2 a migration target
cOld.z = 2.5;

// Larger than the eager-inline bytecode limit.
function noop() {
  let n = 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  n += 0;
  return n;
}
%PrepareFunctionForOptimization(noop);

function polyQ(o) {
  noop(); // Not inlined
  // MigrateMapIfNeeded, then a polymorphic branch with MC2 and MD1 (in this
  // order).
  return o.q;
}
%PrepareFunctionForOptimization(polyQ);

polyQ(cTarget);
polyQ(dOld);

let smi_array = [0, 0];

function victim(cObj, dObj) {
  // CheckMapsWithMigration(MC2) cObj; tries to migrate to MC2 (but the cTarget we
  // pass already has MC2).
  cObj.q;

  // CheckMaps(MD1) dObj
  dObj.p;

  // Eventually we'l inline polyQ here. Here we need to conservatively assume
  // that the noop call might change the maps, so we get:
  // CallFunction noop
  // MigrateMapIfNeeded dObj
  // Polymoprhic branch (MC2, MD1):
  // /-- Branch if dObj's map is MC2
  // |      |
  // |      v
  // |   CheckMap(MD1) dObj <<< Load-bearing! Must not be eliminated
  // |      |
  // |      v
  // \--> Result unused, so this is empty
  polyQ(dObj);

  // The map better be MD1 or this goes wrong.
  smi_array[0] = dObj.x;
}
%PrepareFunctionForOptimization(victim);
victim(cTarget, dOld);

%OptimizeMaglevOnNextCall(victim);

victim(cTarget, dOld);

// Move the smi_array backing store to old space before storing a young pointer.
gc();

// Create a new map MD3 where x is a HeapNumber; MD1 deprecated.
new D(1.5);

// Now the MigrateMapIfNeeded will migrate dOld to MD3 where x is a HeapNumber.
victim(cTarget, dOld);
