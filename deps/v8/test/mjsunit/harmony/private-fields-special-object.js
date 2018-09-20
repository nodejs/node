// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-private-fields --allow-natives-syntax

async function f(assert) {
  try {
    module_namespace_obj = await import('modules-skip-1.js');
  } catch(e) {
    %AbortJS(e);
  }

  class A {
    constructor(arg) {
      return arg;
    }
  }

  class X extends A {
    #x = 1;

    constructor(arg) {
      super(arg);
    }

    getX(arg) {
      return arg.#x;
    }

    setX(arg, val) { arg.#x = val; }
  }

  let x = new X(module_namespace_obj);

  assert.equals(1, X.prototype.getX(module_namespace_obj));
  assert.equals(1, X.prototype.getX(module_namespace_obj));
  assert.equals(1, X.prototype.getX(module_namespace_obj));

  X.prototype.setX(module_namespace_obj, 2);
  X.prototype.setX(module_namespace_obj, 3);
  X.prototype.setX(module_namespace_obj, 4);
}

testAsync(assert => {
  assert.plan(3);

  f(assert).catch(assert.unreachable);
}, "private-fields-special-object");
