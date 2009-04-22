// Copyright 2008 the V8 project authors. All rights reserved.
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

// Test array sort.

// Test counter-intuitive default number sorting.
function TestNumberSort() {
  var a = [ 200, 45, 7 ];

  // Default sort converts each element to string and orders
  // lexicographically.
  a.sort();
  assertArrayEquals([ 200, 45, 7 ], a);
  // Sort numbers by value using a compare functions.
  a.sort(function(x, y) { return x - y; });
  assertArrayEquals([ 7, 45, 200 ], a);

  // Default sort on negative numbers.
  a = [-12345,-123,-1234,-123456];
  a.sort();
  assertArrayEquals([-123,-1234,-12345,-123456], a);

  // Default sort on negative and non-negative numbers.
  a = [123456,0,-12345,-123,123,1234,-1234,0,12345,-123456];
  a.sort();
  assertArrayEquals([-123,-1234,-12345,-123456,0,0,123,1234,12345,123456], a);

  // Tricky case avoiding integer overflow in Runtime_SmiLexicographicCompare.
  a = [9, 1000000000].sort();
  assertArrayEquals([1000000000, 9], a);
  a = [1000000000, 1].sort();
  assertArrayEquals([1, 1000000000], a);
  a = [1000000000, 0].sort();
  assertArrayEquals([0, 1000000000], a);

  // One string is a prefix of the other.
  a = [1230, 123].sort();
  assertArrayEquals([123, 1230], a);
  a = [1231, 123].sort();
  assertArrayEquals([123, 1231], a);

  // Default sort on Smis and non-Smis.
  a = [1000000000, 10000000000, 1000000001, -1000000000, -10000000000, -1000000001];
  a.sort();
  assertArrayEquals([-1000000000, -10000000000, -1000000001, 1000000000, 10000000000, 1000000001], a);


  for (var xb = 1; xb <= 1000 * 1000 * 1000; xb *= 10) {
    for (var xf = 0; xf <= 9; xf++) {
      for (var xo = -1; xo <= 1; xo++) {
        for (var yb = 1; yb <= 1000 * 1000 * 1000; yb *= 10) {
          for (var yf = 0; yf <= 9; yf++) {
            for (var yo = -1; yo <= 1; yo++) {
              var x = xb * xf + xo;
              var y = yb * yf + yo;
              if (!%_IsSmi(x)) continue;
              if (!%_IsSmi(y)) continue;
              var lex = %SmiLexicographicCompare(x, y);
              if (lex < 0) lex = -1;
              if (lex > 0) lex = 1;
              assertEquals(lex, (x == y) ? 0 : ((x + "") < (y + "") ? -1 : 1), x + " < " + y);
            }
          }
        }
      }
    }
  }
}

TestNumberSort();


// Test lexicographical string sorting.
function TestStringSort() {
  var a = [ "cc", "c", "aa", "a", "bb", "b", "ab", "ac" ];
  a.sort();
  assertArrayEquals([ "a", "aa", "ab", "ac", "b", "bb", "c", "cc" ], a);
}

TestStringSort();


// Test object sorting.  Calls toString on each element and sorts
// lexicographically.
function TestObjectSort() {
  var obj0 = { toString: function() { return "a"; } };
  var obj1 = { toString: function() { return "b"; } };
  var obj2 = { toString: function() { return "c"; } };
  var a = [ obj2, obj0, obj1 ];
  a.sort();
  assertArrayEquals([ obj0, obj1, obj2 ], a);
}

TestObjectSort();

// Test array sorting with holes in the array.
function TestArraySortingWithHoles() {
  var a = [];
  a[4] = "18";
  a[10] = "12";
  a.sort();
  assertEquals(11, a.length);
  assertEquals("12", a[0]);
  assertEquals("18", a[1]);
}

TestArraySortingWithHoles();

// Test array sorting with undefined elemeents in the array.
function TestArraySortingWithUndefined() {
  var a = [ 3, void 0, 2 ];
  a.sort();
  assertArrayEquals([ 2, 3, void 0 ], a);
}

TestArraySortingWithUndefined();

// Test that sorting using an unsound comparison function still gives a
// sane result, i.e. it terminates without error and retains the elements
// in the array.
function TestArraySortingWithUnsoundComparisonFunction() {
  var a = [ 3, void 0, 2 ];
  a.sort(function(x, y) { return 1; });
  a.sort();
  assertArrayEquals([ 2, 3, void 0 ], a);
}

TestArraySortingWithUnsoundComparisonFunction();
