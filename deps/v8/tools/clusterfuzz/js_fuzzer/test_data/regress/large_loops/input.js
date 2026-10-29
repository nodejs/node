// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var x = 0;
var y = 0;

for (const a = 0; a < 10000; a++) {
  console.log();
}

for (const a = 0; 1e5 >= a; a--) {
  console.log();
}

for (const a = -10000; a < 0; a++) {
  console.log();
}

for (const a = 0n; a < 10000n; a++) {
  console.log();
}

for (const a = -0.3; a < 1000.1; a += 0.5) {
  console.log();
}
