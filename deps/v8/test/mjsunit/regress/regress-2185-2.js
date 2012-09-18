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

// These tests used to time out before this was fixed.

var LEN = 2e4;

function short() {
  var sum = 0;
  for (var i = 0; i < 1000; i++) {
    var a = [1, 4, 34, 23, 6, 123, 3, 2, 11, 515, 4, 33, 22, 2, 2, 1, 0, 123,
            23, 42, 43, 1002, 44, 43, 101, 23, 55, 11, 101, 102, 45, 11, 404,
            31415, 34, 53, 453, 45, 34, 5, 2, 35, 5, 345, 36, 45, 345, 3, 45,
            3, 5, 5, 2, 2342344, 2234, 23, 2718, 1500, 2, 19, 22, 43, 41, 0,
            -1, 33, 45, 78];
    a.sort(function(a, b) { return a - b; });
    sum += a[0];
  }
  return sum;
}

function short_bench(name, array) {
  var start = new Date();
  short();
  var end = new Date();
  var ms = end - start;
  print("Short " + Math.floor(ms) + "ms");
}

function sawseq(a, tooth) {
  var count = 0;
  while (true) {
    for (var i = 0; i < tooth; i++) {
      a.push(i);
      if (++count >= LEN) return a;
    }
  }
}

function sawseq2(a, tooth) {
  var count = 0;
  while (true) {
    for (var i = 0; i < tooth; i++) {
      a.push(i);
      if (++count >= LEN) return a;
    }
    for (var i = 0; i < tooth; i++) {
      a.push(tooth - i);
      if (++count >= LEN) return a;
    }
  }
}

function sawseq3(a, tooth) {
  var count = 0;
  while (true) {
    for (var i = 0; i < tooth; i++) {
      a.push(tooth - i);
      if (++count >= LEN) return a;
    }
  }
}

function up(a) {
  for (var i = 0; i < LEN; i++) {
    a.push(i);
  }
  return a;
}

function down(a) {
  for (var i = 0; i < LEN; i++) {
    a.push(LEN - i);
  }
  return a;
}

function ran(a) {
  for (var i = 0; i < LEN; i++) {
    a.push(Math.floor(Math.random() * LEN));
  }
  return a;
}

var random = ran([]);
var asc = up([]);
var desc = down([]);
var asc_desc = down(up([]));
var desc_asc = up(down([]));
var asc_asc = up(up([]));
var desc_desc = down(down([]));
var saw1 = sawseq([], 1000);
var saw2 = sawseq([], 500);
var saw3 = sawseq([], 200);
var saw4 = sawseq2([], 200);
var saw5 = sawseq3([], 200);

function bench(name, array) {
  var start = new Date();
  array.sort(function(a, b) { return a - b; });
  var end = new Date();
  for (var i = 0; i < array.length - 1; i++) {
    if (array[i] > array[i + 1]) throw name + " " + i;
  }
  var ms = end - start;
  print(name + " " + Math.floor(ms) + "ms");
}

short_bench();
bench("random", random);
bench("up", asc);
bench("down", desc);
bench("saw 1000", saw1);
bench("saw 500", saw2);
bench("saw 200", saw3);
bench("saw 200 symmetric", saw4);
bench("saw 200 down", saw4);
bench("up, down", asc_desc);
bench("up, up", asc_asc);
bench("down, down", desc_desc);
bench("down, up", desc_asc);
