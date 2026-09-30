// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

let victims = [];
for (let i = 0; i < 2000; i++) {
  victims.push(eval("(function victim_" + i + "() {})"));
}

globalThis.receive = function(message) {};

if (typeof send === 'function') {
  send(JSON.stringify({id: 1, method: "Runtime.enable"}));

  let code = `
    (function() {
      let keep = [];
      for (let i = 0; i < 50000; i++) {
        keep.push({a: i});
        keep.push({a: i, b: i});
        keep.push([i, i]);
      }
      keep = null;
      for (let i = 0; i < 100000; i++) {
        let dummy = {c: i};
      }
      for (let j = 0; j < 2000; j++) {
        try {
          let proto = victims[j].prototype;
          proto.polluted = true;
        } catch (e) {}
      }
    })()
  `;

  send(JSON.stringify({
    id: 2,
    method: "Runtime.evaluate",
    params: {
      expression: code,
      throwOnSideEffect: true
    }
  }));
}

let polluted = 0;
for (let i = 0; i < 2000; i++) {
  if (victims[i].prototype && victims[i].prototype.polluted) {
    polluted++;
  }
}

assertEquals(0, polluted);
