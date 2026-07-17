// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = 'a'.repeat(32);
var b = 'b'.repeat(%StringMaxLength() / 32 + 1);

function replace1() {
  a.replace(/./g, b);
}

assertThrows(replace1, RangeError);


var a = 'a'.repeat(Math.sqrt(%StringMaxLength()) + 1);

function replace2() {
  a.replace(/a/g, a);
}

assertThrows(replace2, RangeError);
