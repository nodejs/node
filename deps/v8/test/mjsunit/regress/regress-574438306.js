// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const props = [];
for (let i = 0; i < 100; i++) {
  props.push(`p${i}: undefined`);
}

const w = new Worker(
    `
  const o = {${props.join(', ')}};
  const inner = new Array(4000).fill(o);
  const outer = new Array(4000).fill(inner);
  const root = new Array(20).fill(outer);
  postMessage('start');
  JSON.stringify(root);
`,
    {type: 'string'});

assertEquals('start', w.getMessage());
const spin = Date.now();
while (Date.now() - spin < 20) {
}
w.terminateAndWait();
