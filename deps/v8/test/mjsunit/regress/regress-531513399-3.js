// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

class ParentClass {
  get prop() {
    return this.leak;
  }
}

class ChildClass extends ParentClass {
  m() {
    return super.prop;
  }
  mKeyed(k) {
    return super[k];
  }
}

// Strict JS getters (class getters) receive undefined as this when called with
// undefined receiver, throwing TypeError when accessing properties on this.
assertThrows(() => ChildClass.prototype.m.call(undefined), TypeError);
assertThrows(
    () => ChildClass.prototype.mKeyed.call(undefined, 'prop'), TypeError);
