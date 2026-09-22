// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

globalThis.leak = 'leak_value';

function ParentSloppy() {}
Object.defineProperty(ParentSloppy.prototype, 'prop', {
  get: function() {
    return this.leak;
  },
  configurable: true
});

class ChildSloppy extends ParentSloppy {
  m() {
    return super.prop;
  }
  mKeyed(k) {
    return super[k];
  }
}

// Sloppy JS getters convert undefined receiver to globalThis (reading
// globalThis.leak).
assertEquals('leak_value', ChildSloppy.prototype.m.call(undefined));
assertEquals(
    'leak_value', ChildSloppy.prototype.mKeyed.call(undefined, 'prop'));
