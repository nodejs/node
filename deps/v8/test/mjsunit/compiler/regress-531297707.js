// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector

let msgId = 1;
function cmd(method, params) {
  return JSON.stringify({id: msgId++, method: method, params: params || {}});
}

let ab = new ArrayBuffer(0x40000);   // 256 KiB
let ta = new Uint32Array(ab);

let armDetach = false;

globalThis.handleInspectorMessage = function() {
  if (armDetach) {
    ab.transfer(0);
  }
  send(cmd("Debugger.resume"));
};

send(cmd("Debugger.enable"));
const PAUSE = cmd("Debugger.pause");

function hot(ta) {
  send(PAUSE);
  let sum = 0;
  const n = ta.length;
  for (let i = 0; i < n; i++) {
    sum += ta[i];
    if (i > 10) break; // Early exit keeps it fast when deoptimized
  }
  return sum;
}

%PrepareFunctionForOptimization(hot);
hot(ta);
%OptimizeFunctionOnNextCall(hot);
hot(ta); // Trigger optimization

armDetach = true;
hot(ta); // Should deopt and not crash
