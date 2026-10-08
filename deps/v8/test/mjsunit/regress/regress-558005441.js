// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

for (let i = 0; i < 2; i++) {
  let w1 = new Worker(`
    let w2 = new Worker(\`
      let ab = new ArrayBuffer(1024);
      postMessage(ab, [ab]);
    \`, {type: "string"});
  `, {type: "string"});
  let w3 = new Worker(`
  `, {type: "string"});
}
