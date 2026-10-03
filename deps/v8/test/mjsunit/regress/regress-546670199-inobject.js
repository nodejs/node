// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic
// Flags: --max-valid-polymorphic-map-count=4

'use strict';

// 9 constructors ensure we exceed the polymorphic IC limit (4) and fill the
// homomorphic IC cache (8).
const constructors = Array.from({length: 9}, () => new Function(''));
const targetIndex = constructors.length - 1;

// Only in-object properties 'x' and 'y' (no out-of-object properties).
function decorate(o, x, y) {
  o.x = x;
  o.y = y;
  return o;
}

function victim(o, storeY, value) {
  const y_box = o.y;
  if (storeY) {
    o.y = value;
  }
  return y_box;
}

const staleSmi = 0x3fffffff;

// Allocate objects before deprecation so their 'y' field has Smi
// representation.
const shapes = constructors.map((C, i) => decorate(new C(), 100 + i, staleSmi));
const toMigrate = decorate(new constructors[targetIndex](), 112, staleSmi);
const unmigrated = decorate(new constructors[targetIndex](), 112, staleSmi);

%PrepareFunctionForOptimization(victim);

// 1. Train o.y as homomorphic in-object load across all maps.
for (const s of shapes) {
  victim(s, false, 0);
}

// 2. Train o.y store on target map while 'y' still has Smi representation.
victim(shapes[targetIndex], true, staleSmi);

// 3. Deprecate target map by storing a Double to 'y'.
shapes[targetIndex].y = 1.25;

// 4. Trigger IC migration on toMigrate to mark target map with
//    is_migration_target=true.
void toMigrate.y;

// 5. Optimize victim with TurboFan.
%OptimizeFunctionOnNextCall(victim);
victim(shapes[targetIndex], false, 0);

// 6. Trigger: Passing 'unmigrated' causes CheckMaps(kTryMigrateInstance) to
//    migrate the in-object field 'y' from Smi to Double, allocating a
//    HeapNumber box at the in-object offset.
victim(unmigrated, true, 13);
assertEquals(13, unmigrated.y);
assertEquals(112, unmigrated.x);
