// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = "a";
for (var i = 0; i < 5; i++) a += a;
var b = "b";
for (var i = 0; i < 23; i++) b += b;

function replace() {
  a.replace(/a/g, function() { return b });
}

assertThrows(replace, RangeError);
