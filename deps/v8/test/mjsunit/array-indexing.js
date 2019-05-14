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

var array = [1,2,3,1,2,3,1,2,3,1,2,3];
var undef_array = [0,,2,undefined,4,,6,undefined,8,,10];
// Sparse arrays with length 42000.
var sparse_array = [];
sparse_array[100] = 3;
sparse_array[200] = undefined;
sparse_array[300] = 4;
sparse_array[400] = 5;
sparse_array[500] = 6;
sparse_array[600] = 5;
sparse_array[700] = 4;
sparse_array[800] = undefined;
sparse_array[900] = 3
sparse_array[41999] = "filler";

var dense_object = { 0: 42, 1: 37, length: 2 };
var sparse_object = { 0: 42, 100000: 37, length: 200000 };
var funky_object = { 10:42, 100000: 42, 100001: 37, length: 50000 };
var infinite_object = { 10: 42, 100000: 37, length: Infinity };

// ----------------------------------------------------------------------
// Array.prototype.indexOf.
// ----------------------------------------------------------------------

// Negative cases.
assertEquals(-1, [].indexOf(1));
assertEquals(-1, array.indexOf(4));
assertEquals(-1, array.indexOf(3, array.length));

assertEquals(2, array.indexOf(3));
// Negative index out of range.
assertEquals(0, array.indexOf(1, -17));
// Negative index in rage.
assertEquals(3, array.indexOf(1, -11));
// Index in range.
assertEquals(3, array.indexOf(1, 1));
assertEquals(3, array.indexOf(1, 3));
assertEquals(6, array.indexOf(1, 4));

// Find undefined, not holes.
assertEquals(3, undef_array.indexOf(undefined));
assertEquals(3, undef_array.indexOf(undefined, 3));
assertEquals(7, undef_array.indexOf(undefined, 4));
assertEquals(7, undef_array.indexOf(undefined, 7));
assertEquals(-1, undef_array.indexOf(undefined, 8));
assertEquals(3, undef_array.indexOf(undefined, -11));
assertEquals(3, undef_array.indexOf(undefined, -8));
assertEquals(7, undef_array.indexOf(undefined, -7));
assertEquals(7, undef_array.indexOf(undefined, -4));
assertEquals(-1, undef_array.indexOf(undefined, -3));

// Find in sparse array.
assertEquals(100, sparse_array.indexOf(3));
assertEquals(900, sparse_array.indexOf(3, 101));
assertEquals(-1, sparse_array.indexOf(3, 901));
assertEquals(100, sparse_array.indexOf(3, -42000));
assertEquals(900, sparse_array.indexOf(3, 101 - 42000));
assertEquals(-1, sparse_array.indexOf(3, 901 - 42000));

assertEquals(300, sparse_array.indexOf(4));
assertEquals(700, sparse_array.indexOf(4, 301));
assertEquals(-1, sparse_array.indexOf(4, 701));
assertEquals(300, sparse_array.indexOf(4, -42000));
assertEquals(700, sparse_array.indexOf(4, 301 - 42000));
assertEquals(-1, sparse_array.indexOf(4, 701 - 42000));

assertEquals(200, sparse_array.indexOf(undefined));
assertEquals(800, sparse_array.indexOf(undefined, 201));
assertEquals(-1, sparse_array.indexOf(undefined, 801));
assertEquals(200, sparse_array.indexOf(undefined, -42000));
assertEquals(800, sparse_array.indexOf(undefined, 201 - 42000));
assertEquals(-1, sparse_array.indexOf(undefined, 801 - 42000));

// Find in non-arrays.
assertEquals(0, Array.prototype.indexOf.call(dense_object, 42));
assertEquals(1, Array.prototype.indexOf.call(dense_object, 37));
assertEquals(-1, Array.prototype.indexOf.call(dense_object, 87));

assertEquals(0, Array.prototype.indexOf.call(sparse_object, 42));
assertEquals(100000, Array.prototype.indexOf.call(sparse_object, 37));
assertEquals(-1, Array.prototype.indexOf.call(sparse_object, 87));

assertEquals(10, Array.prototype.indexOf.call(funky_object, 42));
assertEquals(-1, Array.prototype.indexOf.call(funky_object, 42, 15));
assertEquals(-1, Array.prototype.indexOf.call(funky_object, 37));

assertEquals(10, Array.prototype.indexOf.call(infinite_object, 42));

// ----------------------------------------------------------------------
// Array.prototype.lastIndexOf.
// ----------------------------------------------------------------------

// Negative cases.
assertEquals(-1, [].lastIndexOf(1));
assertEquals(-1, array.lastIndexOf(1, -17));

assertEquals(9, array.lastIndexOf(1));
// Index out of range.
assertEquals(9, array.lastIndexOf(1, array.length));
// Index in range.
assertEquals(0, array.lastIndexOf(1, 2));
assertEquals(3, array.lastIndexOf(1, 4));
assertEquals(3, array.lastIndexOf(1, 3));
// Negative index in range.
assertEquals(0, array.lastIndexOf(1, -11));

// Find undefined, not holes.
assertEquals(7, undef_array.lastIndexOf(undefined));
assertEquals(-1, undef_array.lastIndexOf(undefined, 2));
assertEquals(3, undef_array.lastIndexOf(undefined, 3));
assertEquals(3, undef_array.lastIndexOf(undefined, 6));
assertEquals(7, undef_array.lastIndexOf(undefined, 7));
assertEquals(7, undef_array.lastIndexOf(undefined, -1));
assertEquals(-1, undef_array.lastIndexOf(undefined, -9));
assertEquals(3, undef_array.lastIndexOf(undefined, -8));
assertEquals(3, undef_array.lastIndexOf(undefined, -5));
assertEquals(7, undef_array.lastIndexOf(undefined, -4));

// Find in sparse array.
assertEquals(900, sparse_array.lastIndexOf(3));
assertEquals(100, sparse_array.lastIndexOf(3, 899));
assertEquals(-1, sparse_array.lastIndexOf(3, 99));
assertEquals(900, sparse_array.lastIndexOf(3, -1));
assertEquals(100, sparse_array.lastIndexOf(3, 899 - 42000));
assertEquals(-1, sparse_array.lastIndexOf(3, 99 - 42000));

assertEquals(700, sparse_array.lastIndexOf(4));
assertEquals(300, sparse_array.lastIndexOf(4, 699));
assertEquals(-1, sparse_array.lastIndexOf(4, 299));
assertEquals(700, sparse_array.lastIndexOf(4, -1));
assertEquals(300, sparse_array.lastIndexOf(4, 699 - 42000));
assertEquals(-1, sparse_array.lastIndexOf(4, 299 - 42000));

assertEquals(800, sparse_array.lastIndexOf(undefined));
assertEquals(200, sparse_array.lastIndexOf(undefined, 799));
assertEquals(-1, sparse_array.lastIndexOf(undefined, 199));
assertEquals(800, sparse_array.lastIndexOf(undefined, -1));
assertEquals(200, sparse_array.lastIndexOf(undefined, 799 - 42000));
assertEquals(-1, sparse_array.lastIndexOf(undefined, 199 - 42000));

assertEquals(0, Array.prototype.lastIndexOf.call(dense_object, 42));
assertEquals(1, Array.prototype.lastIndexOf.call(dense_object, 37));
assertEquals(0, Array.prototype.lastIndexOf.call(sparse_object, 42));
assertEquals(100000, Array.prototype.lastIndexOf.call(sparse_object, 37));

//Find in non-arrays.
assertEquals(0, Array.prototype.lastIndexOf.call(dense_object, 42));
assertEquals(1, Array.prototype.lastIndexOf.call(dense_object, 37));
assertEquals(-1, Array.prototype.lastIndexOf.call(dense_object, 87));

assertEquals(0, Array.prototype.lastIndexOf.call(sparse_object, 42));
assertEquals(100000, Array.prototype.lastIndexOf.call(sparse_object, 37));
assertEquals(-1, Array.prototype.lastIndexOf.call(sparse_object, 87));

assertEquals(10, Array.prototype.lastIndexOf.call(funky_object, 42, 15));
assertEquals(10, Array.prototype.lastIndexOf.call(funky_object, 42));
assertEquals(-1, Array.prototype.lastIndexOf.call(funky_object, 37));

// This call would take too long because it would search backwards from 2**53-1
// assertEquals(-1, Array.prototype.lastIndexOf.call(infinite_object, 42));
