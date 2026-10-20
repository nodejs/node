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

// Fill in-object properties so 'x' and 'y' are placed in out-of-object
// PropertyArray.
function decorate(object, x, y) {
  object.p0 = 0;
  object.p1 = 1;
  object.p2 = 2;
  object.p3 = 3;
  object.p4 = 4;
  object.p5 = 5;
  object.p6 = 6;
  object.p7 = 7;
  object.p8 = 8;
  object.p9 = 9;
  object.x = x;
  object.y = y;
  return object;
}

function victim(o, storeY, value) {
  const x = o.x;
  if (storeY) {
    o.y = value;
  }
  return x;
}

const staleSmi = 0x3fffffff;

// Allocate objects before deprecation so their 'y' field has Smi
// representation.
const shapes = constructors.map((C, i) => decorate(new C(), 100 + i, staleSmi));
const toMigrate = decorate(new constructors[targetIndex](), 112, staleSmi);
const unmigrated = decorate(new constructors[targetIndex](), 112, staleSmi);

%PrepareFunctionForOptimization(victim);

// 1. Train o.x as homomorphic across all maps.
for (const s of shapes) {
  victim(s, false, 0);
}

// 2. Train o.y on target map while 'y' still has Smi representation.
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
//    migrate the instance to the double-field map and allocate a new
//    PropertyArray.
victim(unmigrated, true, 13);
assertEquals(13, unmigrated.y);
assertEquals(112, unmigrated.x);
