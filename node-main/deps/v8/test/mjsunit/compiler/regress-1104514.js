// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Array(32760);  // > JSArray::kInitialMaxFastElementArray

function main() {
  const a = [1, 2];
  a.x = 666;
  a.toString();
  const aa = Array.prototype.map.call(a, v => v);
  if (aa[0] != 1 || aa[1] != 2) { %SystemBreak(); }
  a.z = 667;
}

for (var i = 0; i < 20000; ++i) {
  main();
}
