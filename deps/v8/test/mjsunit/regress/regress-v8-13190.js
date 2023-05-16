// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --expose-gc
// Flags: --no-sparkplug --no-maglev --no-turbofan
// Flags: --lazy --flush-bytecode

// Helper that, given a template object, simply returns it.
function get_template_object(obj) {
  return obj;
}

function foo_factory() {
  // The SFI for foo is held by the bytecode array of foo_factory, so will be
  // collected if foo_factory's bytecode is flushed.
  return function foo() {
    return get_template_object``
  }
}

// Create foo in another function to avoid it leaking on the global object or
// top-level script locals, and accidentally being kept alive.
function get_foo_template_object() {
  let foo = foo_factory();
  return foo();
}

assertTrue(isLazy(foo_factory));

let inital_template_object = get_foo_template_object();
assertEquals(inital_template_object, get_foo_template_object(),
             "Template object identity should be preserved");

// Force a flush of foo_factory, so that the SharedFunctionInfo of foo can be
// collected.
%ForceFlush(foo_factory);
assertTrue(isLazy(foo_factory));

// Do a few more GCs to allow weak maps to be cleared.
gc();
gc();
gc();

// Flushing foo_factory and GCing foo should not affect the persisted reference
// identity of the template object inside foo.
assertSame(inital_template_object, get_foo_template_object(),
           "Template object identity should be preserved");
