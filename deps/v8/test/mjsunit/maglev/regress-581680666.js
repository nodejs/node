// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function loadProperty(obj, key) {
  return obj[key];
}

const emptyObj = {};

// A non-internalized string constant ("a" + "b.c") becomes a ThinString in the
// constant pool once internalized on the first IC miss. When Maglev compiles
// this call while the KeyedLoadIC slot is MEGAMORPHIC, it refines the access to
// a named Builtin::kLoadIC_Megamorphic call.
function loadWithThinStringKey(obj) {
  return loadProperty(obj, "a" + "b.c");
}

const proxyWithTrap = new Proxy({}, {
  get() {
    loadProperty(emptyObj, "x");
    loadWithThinStringKey(emptyObj);
    %OptimizeFunctionOnNextCall(loadWithThinStringKey);
    loadWithThinStringKey(emptyObj);
    return "pass";
  }
});

%PrepareFunctionForOptimization(loadProperty);
%PrepareFunctionForOptimization(loadWithThinStringKey);
assertEquals("pass", loadProperty(proxyWithTrap, 0));
assertEquals(undefined, loadWithThinStringKey(emptyObj));

function loadElementOrProperty(obj, key) {
  return obj[key];
}

function loadWithThinStringKeyFromElementGetter(obj) {
  return loadElementOrProperty(obj, "d" + "e.f");
}

const objWithElementGetter = {
  get 0() {
    // Transition loadElementOrProperty's KeyedLoadIC slot to MEGAMORPHIC and
    // compile loadWithThinStringKeyFromElementGetter while the outer element
    // load is still inside KeyedLoadIC::RuntimeLoad. When the getter returns,
    // UpdateLoadElement must not overwrite the MEGAMORPHIC slot back to
    // MONOMORPHIC.
    loadElementOrProperty(emptyObj, "x");
    loadWithThinStringKeyFromElementGetter(emptyObj);
    %OptimizeFunctionOnNextCall(loadWithThinStringKeyFromElementGetter);
    loadWithThinStringKeyFromElementGetter(emptyObj);
    return "pass";
  }
};

%PrepareFunctionForOptimization(loadElementOrProperty);
%PrepareFunctionForOptimization(loadWithThinStringKeyFromElementGetter);
assertEquals("pass", loadElementOrProperty(objWithElementGetter, 0));
assertEquals(undefined, loadWithThinStringKeyFromElementGetter(emptyObj));

// Re-entrantly transitioning a MONOMORPHIC element IC to POLYMORPHIC inside an
// element getter must not be clobbered back to MONOMORPHIC by a stale cached
// IC::state() when the outer RuntimeLoad returns.
function keyedLoadPolymorphic(obj, idx) {
  return obj[idx];
}

const smiArray = [1, 2];
const int32Array = new Int32Array([10, 20]);
const holeyDoubleArray = [1.5, , 2.5];
Object.defineProperty(Array.prototype, 1, {
  get() {
    // Transition keyedLoadPolymorphic from MONOMORPHIC (PACKED_SMI_ELEMENTS) to
    // POLYMORPHIC (PACKED_SMI_ELEMENTS, INT32_ELEMENTS) while the outer call on
    // holeyDoubleArray (HOLEY_DOUBLE_ELEMENTS, a more general elements kind
    // than PACKED_SMI_ELEMENTS) is inside RuntimeLoad.
    keyedLoadPolymorphic(int32Array, 0);
    return 42;
  },
  configurable: true,
});

keyedLoadPolymorphic(smiArray, 0);
assertEquals(42, keyedLoadPolymorphic(holeyDoubleArray, 1));
assertEquals(10, keyedLoadPolymorphic(int32Array, 0));
delete Array.prototype[1];
