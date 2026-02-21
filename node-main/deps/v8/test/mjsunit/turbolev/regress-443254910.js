// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --jit-fuzzing

function __isPropertyOfType( name) {
  try { name; } catch (e) {}
  return typeof type === 'undefined' || typeof desc.value === type;
}

function __getProperties(obj) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
    if (__isPropertyOfType()) properties.name;
  }
}

function* __getObjects(root = this, level = 0) {
  __getProperties(root);
}

function __getRandomObject () {
  for (let obj of __getObjects()) {}
}

function __wrapTC(f = true) {
    return f();
}

class Base {
  static toString() {
    __getRandomObject();
  }
}
class C {
  toString() {
    try {
      this.propertyIsEnumerable(this);
    } catch {}
    Base + "a";
  }
}

const __v_2 = new C();
const __v_3 = [__v_2];
const __v_4 = __wrapTC(() => ({
  [__v_3]: C
}));
