// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function fastaRandom(n, table) {
  var line = new Array(5);
  while (n > 0) {
    if (n < line.length) line = new Array(n);
    %OptimizeOsr();
    line[0] = n;
    n--;
  }
}

print("---BEGIN 1");
assertEquals(undefined, fastaRandom(6, null));
print("---BEGIN 2");
assertEquals(undefined, fastaRandom(6, null));
print("---END");
