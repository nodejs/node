// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noalways-opt

var elements_kind = {
  fast_smi_only            :  'fast smi only elements',
  fast                     :  'fast elements',
  fast_double              :  'fast double elements',
  dictionary               :  'dictionary elements',
}

function getKind(obj) {
  if (%HasFastSmiElements(obj)) return elements_kind.fast_smi_only;
  if (%HasFastObjectElements(obj)) return elements_kind.fast;
  if (%HasFastDoubleElements(obj)) return elements_kind.fast_double;
  if (%HasDictionaryElements(obj)) return elements_kind.dictionary;
}

function assertKind(expected, obj, name_opt) {
  assertEquals(expected, getKind(obj), name_opt);
}

(function() {
  function make1() { return new Array(); }
  function make2() { return new Array(); }
  function make3() { return new Array(); }
  function foo(a, i) { a[0] = i; }

  function run_test(maker_function) {
    var one = maker_function();
    assertKind(elements_kind.fast_smi_only, one);
    // Use memento to pre-transition allocation site to DOUBLE elements.
    foo(one, 1.5);
    // Newly created arrays should now have DOUBLE elements right away.
    var two = maker_function();
    assertKind(elements_kind.fast_double, two);
  }

  // Initialize the KeyedStoreIC in foo; the actual operation will be done
  // in the runtime.
  run_test(make1);
  // Run again; the IC optimistically assumed to only see the transitioned
  // (double-elements) map again, so this will make it polymorphic.
  // The actual operation will again be done in the runtime.
  run_test(make2);
  // Finally, check if the initialized IC honors the allocation memento.
  run_test(make3);
})();
