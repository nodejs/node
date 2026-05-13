// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let key = 'AA';
let value = 2;
class C extends Function {
  [key] = value;
}

// First object creation triggers transition MapA----(AA, kData, SMI)--->MapB
let o1 = new C('\'use strict\'');

value = 1.1;

// Second object creation triggers TryFastAddDataProperty() which reconfigures
// the property from Smi to Double. This reconfigures MapB in-place (or
// normalizes it). TryFastAddDataProperty must not crash if the map is
// normalized during preparation.
let o2 = new C('\'use strict\'');
