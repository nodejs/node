// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-defer-import-eval --turbofan
// Flags: --no-turbolev --no-optimize-on-next-call-optimizes-to-maglev

// A stale module namespace prototype map can get a fresh PrototypeInfo whose
// module_namespace is undefined. MapRef::GetNamespaceModule must not assume
// it is a JSModuleNamespace.
//
// 1. `namespace.then` turns the deferred namespace's map into a fast
//    prototype map M1 without evaluating the module.
// 2. A concurrent Turbofan job serializes `namespace` (with map M1) and is
//    blocked before the inlining phase.
// 3. for-in over an object whose prototype is `namespace` records M1 as the
//    first prototype map, then evaluates the module. Evaluation stamps a
//    private field onto `namespace`, migrating it to M2 and moving the
//    PrototypeInfo along. Afterwards a validity cell is requested for M1,
//    which installs a fresh PrototypeInfo with module_namespace = undefined.
// 4. Resuming the job computes property access info on M1.

(async function main() {
  let source = 'export let x0 = 1;\nglobalThis.onEval();\n';

  let namespace;
  class Base {
    constructor(object) { return object; }
  }
  class Stamp extends Base {
    #field = 1;
  }
  globalThis.onEval = function() {
    new Stamp(namespace);
  };

  namespace = await import.defer('data:text/javascript,' + source);

  function makeFast(o) { return o.then; }
  for (let i = 0; i < 20; ++i) makeFast(namespace);

  function makeDummy() { return function() {}; }
  let dummy0 = makeDummy();
  let dummy = makeDummy();
  dummy.prototype = namespace;

  globalThis.ns_const = namespace;
  function get_x0(o, doLoad) {
    if (doLoad) return o.x0;
  }
  function outer(takeCall, doLoad) {
    if (takeCall) dummy();
    return get_x0(globalThis.ns_const, doLoad);
  }

  %PrepareFunctionForOptimization(get_x0);
  %PrepareFunctionForOptimization(outer);

  // Make `o.x0` megamorphic so the receiver map is inferred from the
  // HeapConstant `namespace`.
  get_x0({x0: 1}, true);
  get_x0({a: 1, x0: 1}, true);
  get_x0({a: 1, b: 1, x0: 1}, true);
  get_x0({a: 1, b: 1, c: 1, x0: 1}, true);
  get_x0({a: 1, b: 1, c: 1, d: 1, x0: 1}, true);

  for (let i = 0; i < 10; ++i) outer(true, false);

  %BlockAt('TurbofanInlining', 5000);
  %OptimizeFunctionOnNextCall(outer, 'concurrent');
  outer(false, false);
  %WaitUntilBlocked('TurbofanInlining', 5000);

  let child = Object.create(namespace);
  for (let k in child) {
    break;
  }

  %Resume('TurbofanInlining');
  %FinalizeOptimization();
  assertEquals(1, outer(false, true));
})();
