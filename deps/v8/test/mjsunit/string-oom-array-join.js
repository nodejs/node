// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = "a".repeat(%StringMaxLength() / 10);
var b = [];
for (var i = 0; i < 11; i++) b.push(a);

function join() {
  b.join();
}

assertThrows(join, RangeError);
