// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



var a = 'a';
for (var i = 0; i < 5; i++) a += a;
var b = 'b';
for (var i = 0; i < 23; i++) b += b;

function replace1() {
  a.replace(/./g, b);
}

assertThrows(replace1, RangeError);


var a = 'a';
for (var i = 0; i < 16; i++) a += a;

function replace2() {
  a.replace(/a/g, a);
}

assertThrows(replace2, RangeError);
