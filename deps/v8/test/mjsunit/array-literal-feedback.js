// Copyright 2013 the V8 project authors. All rights reserved.
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

function get_literal(x) {
  var literal = [1, 2, x];
  return literal;
}

get_literal(3);
// It's important to store a from before we crankshaft get_literal, because
// mementos won't be created from crankshafted code at all.
a = get_literal(3);
  %OptimizeFunctionOnNextCall(get_literal);
get_literal(3);
assertOptimized(get_literal);
assertTrue(%HasFastSmiElements(a));
// a has a memento so the transition caused by the store will affect the
// boilerplate.
a[0] = 3.5;

// We should have transitioned the boilerplate array to double, and
// crankshafted code should de-opt on the unexpected elements kind
b = get_literal(3);
assertTrue(%HasFastDoubleElements(b));
assertEquals([1, 2, 3], b);
assertUnoptimized(get_literal);

// Optimize again
get_literal(3);
  %OptimizeFunctionOnNextCall(get_literal);
b = get_literal(3);
assertTrue(%HasFastDoubleElements(b));
assertOptimized(get_literal);


// Test: make sure allocation site information is updated through a
// transition from SMI->DOUBLE->FAST
(function() {
  function bar(a, b, c) {
    return [a, b, c];
  }

  a = bar(1, 2, 3);
  a[0] = 3.5;
  a[1] = 'hi';
  b = bar(1, 2, 3);
  assertKind(elements_kind.fast, b);
})();
