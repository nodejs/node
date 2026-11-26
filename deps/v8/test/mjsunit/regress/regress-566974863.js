// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const w = new Worker(`
  let n = [];
  for (let i = 0; i < 40; i++) n = [n, n];
  n.flat({
    valueOf() {
      postMessage('start');
      return Infinity;
    }
  });
`, {type: 'string'});

assertEquals('start', w.getMessage());
const spin = Date.now();
while (Date.now() - spin < 20) {}
w.terminateAndWait();
