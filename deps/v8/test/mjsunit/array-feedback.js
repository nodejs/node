// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --noalways-opt

var elements_kind = {
  fast_smi_only            :  'fast smi only elements',
  fast                     :  'fast elements',
  fast_double              :  'fast double elements',
  dictionary               :  'dictionary elements',
  external_byte            :  'external byte elements',
  external_unsigned_byte   :  'external unsigned byte elements',
  external_short           :  'external short elements',
  external_unsigned_short  :  'external unsigned short elements',
  external_int             :  'external int elements',
  external_unsigned_int    :  'external unsigned int elements',
  external_float           :  'external float elements',
  external_double          :  'external double elements',
  external_pixel           :  'external pixel elements'
}

function getKind(obj) {
  if (%HasFastSmiElements(obj)) return elements_kind.fast_smi_only;
  if (%HasFastObjectElements(obj)) return elements_kind.fast;
  if (%HasFastDoubleElements(obj)) return elements_kind.fast_double;
  if (%HasDictionaryElements(obj)) return elements_kind.dictionary;
}

function isHoley(obj) {
  if (%HasFastHoleyElements(obj)) return true;
  return false;
}

function assertKind(expected, obj, name_opt) {
  assertEquals(expected, getKind(obj), name_opt);
}

// Verify that basic elements kind feedback works for non-constructor
// array calls (as long as the call is made through an IC, and not
// a CallStub).
(function (){
  function create0() {
    return Array();
  }

  // Calls through ICs need warm up through uninitialized, then
  // premonomorphic first.
  create0();
  a = create0();
  assertKind(elements_kind.fast_smi_only, a);
  a[0] = 3.5;
  b = create0();
  assertKind(elements_kind.fast_double, b);

  function create1(arg) {
    return Array(arg);
  }

  create1(0);
  create1(0);
  a = create1(0);
  assertFalse(isHoley(a));
  assertKind(elements_kind.fast_smi_only, a);
  a[0] = "hello";
  b = create1(10);
  assertTrue(isHoley(b));
  assertKind(elements_kind.fast, b);

  a = create1(100000);
  assertKind(elements_kind.fast, a);

  function create3(arg1, arg2, arg3) {
    return Array(arg1, arg2, arg3);
  }

  create3(1,2,3);
  create3(1,2,3);
  a = create3(1,2,3);
  a[0] = 3.035;
  assertKind(elements_kind.fast_double, a);
  b = create3(1,2,3);
  assertKind(elements_kind.fast_double, b);
  assertFalse(isHoley(b));
})();


// Verify that keyed calls work
(function (){
  function create0(name) {
    return this[name]();
  }

  name = "Array";
  create0(name);
  create0(name);
  a = create0(name);
  a[0] = 3.5;
  b = create0(name);
  assertKind(elements_kind.fast_double, b);
})();


// Verify that feedback is turned off if the call site goes megamorphic.
(function (){
  function foo(arg) { return arg(); }
  foo(Array);
  foo(function() {});
  foo(Array);

  gc();

  a = foo(Array);
  a[0] = 3.5;
  b = foo(Array);
  // b doesn't benefit from elements kind feedback at a megamorphic site.
  assertKind(elements_kind.fast_smi_only, b);
})();


// Verify that crankshaft consumes type feedback.
(function (){
  function create0() {
    return Array();
  }

  create0();
  create0();
  a = create0();
  a[0] = 3.5;
    %OptimizeFunctionOnNextCall(create0);
  create0();
  create0();
  b = create0();
  assertKind(elements_kind.fast_double, b);
  assertOptimized(create0);

  function create1(arg) {
    return Array(arg);
  }

  create1(8);
  create1(8);
  a = create1(8);
  a[0] = 3.5;
    %OptimizeFunctionOnNextCall(create1);
  b = create1(8);
  assertKind(elements_kind.fast_double, b);
  assertOptimized(create1);

  function createN(arg1, arg2, arg3) {
    return Array(arg1, arg2, arg3);
  }

  createN(1, 2, 3);
  createN(1, 2, 3);
  a = createN(1, 2, 3);
  a[0] = 3.5;
    %OptimizeFunctionOnNextCall(createN);
  b = createN(1, 2, 3);
  assertKind(elements_kind.fast_double, b);
  assertOptimized(createN);
})();

// Verify that cross context calls work
(function (){
  var realmA = Realm.current();
  var realmB = Realm.create();
  assertEquals(0, realmA);
  assertEquals(1, realmB);

  function instanceof_check(type) {
    assertTrue(type() instanceof type);
    assertTrue(type(5) instanceof type);
    assertTrue(type(1,2,3) instanceof type);
  }

  var realmBArray = Realm.eval(realmB, "Array");
  instanceof_check(Array);
  instanceof_check(Array);
  instanceof_check(Array);
  instanceof_check(realmBArray);
  instanceof_check(realmBArray);
  instanceof_check(realmBArray);
})();
