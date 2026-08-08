// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --enable-queue-microtask

// d8 version of wpt/html/webappapis/microtask-queuing/queue-microtask-cross-realm-callback-report-exception.html

// Simulate 3 iframes.
frames = [];
for (let i = 0; i < 3; ++i) {
  frames[i] = Realm.global(Realm.createAllowCrossRealmAccess());
  frames[i].parent = globalThis;
}

const onerrorCalls = [];
globalThis.onerror = function() { onerrorCalls.push("top"); }
frames[0].onerror = () => { onerrorCalls.push("frame0"); };
frames[1].onerror = () => { onerrorCalls.push("frame1"); };
frames[2].onerror = () => { onerrorCalls.push("frame2"); };

(function test() {
  frames[0].queueMicrotask(
    new frames[1].Function(`throw new parent.frames[2].Error("PASS");`),
  );

  setTimeout(() => {
    assertArrayEquals(onerrorCalls, ["frame1"]);
  });
})();
