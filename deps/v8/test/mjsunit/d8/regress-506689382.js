// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-tracing --trace-path=test/mjsunit/d8/regress-506689382-v8-trace.json --trace-config=test/mjsunit/d8/regress-506689382-trace-config.json

'use strict';

function workerMain() {
  onmessage = function({data}) {
    const ctrl = new Int32Array(data.buf);
    Atomics.add(ctrl, 0, 1);
    Atomics.notify(ctrl, 0);
    while (Atomics.load(ctrl, 1) === 0) Atomics.wait(ctrl, 1, 0);

    for (let r = 0; r < 32; r++) {
      for (let i = 0; i < 20; i++) {
        const fn = eval(
            `(function f_${data.id}_${r}_${i}(o){o.x=1;return o.x})` +
            `\n//# sourceURL=s_${data.id}_${r}_${i}.js`);
        for (let j = 0; j < 16; j++) {
          const o = {};
          o['p' + j] = j;
          fn(o);
        }
      }
    }
    postMessage('done');
  };
}

const ctrl = new Int32Array(new SharedArrayBuffer(8));
const workers = [];
for (let i = 0; i < 2; i++) {
  const w = new Worker(workerMain, {type: 'function'});
  w.postMessage({buf: ctrl.buffer, id: i});
  workers.push(w);
}
while (Atomics.load(ctrl, 0) < 2) {
  Atomics.wait(ctrl, 0, Atomics.load(ctrl, 0));
}
Atomics.store(ctrl, 1, 1);
Atomics.notify(ctrl, 1, 2);
for (const w of workers) {
  assertEquals('done', w.getMessage());
  w.terminateAndWait();
}
