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

// Flags: --allow-natives-syntax
// Flags:

// This code exists to eliminate the learning influence of AllocationSites
// on the following tests.
var __sequence = 0;
function make_array_string(length) {
  this.__sequence = this.__sequence + 1;
  return "/* " + this.__sequence + " */  new Array(" + length + ");";
}
function make_array(length) {
  return eval(make_array_string(length));
}

function test(test_double, test_object, set, length) {
  // We apply the same operations to two identical arrays.  The first array
  // triggers an IC miss, upon which the conversion stub is generated, but the
  // actual conversion is done in runtime.  The second array, arriving at
  // the previously patched IC, is then converted using the conversion stub.
  var array_1 = make_array(length);
  var array_2 = make_array(length);

  // false, true, nice setter function, 20
  assertTrue(%HasSmiElements(array_1));
  assertTrue(%HasSmiElements(array_2));
  for (var i = 0; i < length; i++) {
    if (i == length - 5 && test_double) {
      // Trigger conversion to fast double elements at length-5.
      set(array_1, i, 0.5);
      set(array_2, i, 0.5);
      assertTrue(%HasDoubleElements(array_1));
      assertTrue(%HasDoubleElements(array_2));
    } else if (i == length - 3 && test_object) {
      // Trigger conversion to fast object elements at length-3.
      set(array_1, i, 'object');
      set(array_2, i, 'object');
      assertTrue(%HasObjectElements(array_1));
      assertTrue(%HasObjectElements(array_2));
    } else if (i != length - 7) {
      // Set the element to an integer but leave a hole at length-7.
      set(array_1, i, 2*i+1);
      set(array_2, i, 2*i+1);
    }
  }

  for (var i = 0; i < length; i++) {
    if (i == length - 5 && test_double) {
      assertEquals(0.5, array_1[i]);
      assertEquals(0.5, array_2[i]);
    } else if (i == length - 3 && test_object) {
      assertEquals('object', array_1[i]);
      assertEquals('object', array_2[i]);
    } else if (i != length - 7) {
      assertEquals(2*i+1, array_1[i]);
      assertEquals(2*i+1, array_2[i]);
    } else {
      assertEquals(undefined, array_1[i]);
      assertEquals(undefined, array_2[i]);
    }
  }

  assertEquals(length, array_1.length);
  assertEquals(length, array_2.length);
}

function run_test(test_double, test_object, set, length) {
  test(test_double, test_object, set, length);
    %ClearFunctionFeedback(test);
}

run_test(false, false, function(a,i,v){ a[i] = v; }, 20);
run_test(true,  false, function(a,i,v){ a[i] = v; }, 20);
run_test(false, true,  function(a,i,v){ a[i] = v; }, 20);
run_test(true,  true,  function(a,i,v){ a[i] = v; }, 20);

run_test(false, false, function(a,i,v){ a[i] = v; }, 10000);
run_test(true,  false, function(a,i,v){ a[i] = v; }, 10000);
run_test(false, true,  function(a,i,v){ a[i] = v; }, 10000);
run_test(true,  true,  function(a,i,v){ a[i] = v; }, 10000);

// Check COW arrays
function get_cow() { return [1, 2, 3]; }

function transition(x) { x[0] = 1.5; }

var ignore = get_cow();
transition(ignore);  // Handled by runtime.
var a = get_cow();
var b = get_cow();
transition(a);  // Handled by IC.
assertEquals(1.5, a[0]);
assertEquals(1, b[0]);
