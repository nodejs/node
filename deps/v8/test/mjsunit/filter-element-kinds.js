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

// Flags: --allow-natives-syntax --expose-gc --opt --no-always-opt

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
  if (%HasSmiElements(obj)) return elements_kind.fast_smi_only;
  if (%HasObjectElements(obj)) return elements_kind.fast;
  if (%HasDoubleElements(obj)) return elements_kind.fast_double;
  if (%HasDictionaryElements(obj)) return elements_kind.dictionary;
}

function isHoley(obj) {
  if (%HasHoleyElements(obj)) return true;
  return false;
}

function assertKind(expected, obj, name_opt) {
  assertEquals(expected, getKind(obj), name_opt);
}

function assertHoley(obj, name_opt) {
  assertEquals(true, isHoley(obj), name_opt);
}

function assertNotHoley(obj, name_opt) {
  assertEquals(false, isHoley(obj), name_opt);
}

// Create a new closure that inlines Array.prototype.filter().
function create(a) {
  return function() {
    return a.filter(x => false);
  }
}

function runTest(test, kind, holey_predicate) {

  // Verify built-in implementation produces correct results.
  %PrepareFunctionForOptimization(test);
  let a = test();
  assertKind(kind, a);
  holey_predicate(a);
  test();
  test();
  %OptimizeFunctionOnNextCall(test);

  // Now for optimized code.
  a = test();
  assertKind(kind, a);
  holey_predicate(a);
}

function chooseHoleyPredicate(a) {
  return isHoley(a) ? assertHoley : assertNotHoley;
}

(function() {
  let data = [];

  // Packed literal arrays.
  data.push(() => [1, 2, 3]);
  data.push(() => [true, true, false]);
  data.push(() => [1.0, 1.5, 3.5]);
  // Holey literal arrays.
  data.push(() => { let obj = [1,, 3]; obj[1] = 2; return obj; });
  data.push(() => { let obj = [true,, false]; obj[1] = true; return obj; });
  data.push(() => { let obj = [1.0,, 3.5]; obj[1] = 1.5; return obj; });
  // Packed constructed arrays.
  data.push(() => new Array(1, 2, 3));
  data.push(() => new Array(true, true, false));
  data.push(() => new Array(1.0, 1.5, 3.5));

  // Holey constructed arrays.
  data.push(() => {
    let obj = new Array(3);
    obj[0] = 1;
    obj[1] = 2;
    obj[2] = 3;
    return obj;
  });

  data.push(() => {
    let obj = new Array(3);
    obj[0] = true;
    obj[1] = true;
    obj[2] = false;
    return obj;
  });

  data.push(() => {
    let obj = new Array(3);
    obj[0] = 1.0;
    obj[1] = 1.5;
    obj[2] = 3.5;
    return obj;
  });

  for (datum of data) {
    let a = datum();
    // runTest(create(a), getKind(a), chooseHoleyPredicate(a));
    let f = function() { return a.filter(x => false); }
    runTest(f, getKind(a), chooseHoleyPredicate(a));
   }
})();
