// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string --expose-gc

function workerScript() {
  onmessage = function(msg) {
    let sab = new Int32Array(msg.data);

    try {
      %BlockAt("ExternalizeStringExtensionMakeExternalOneByte", 10000);
      %BlockAt("ConcurrentConcatenateStrings", 10000);
      postMessage("ready");

      %WaitUntilBlocked("ConcurrentConcatenateStrings", 10000);
      %WaitUntilBlocked("ExternalizeStringExtensionMakeExternalOneByte", 10000);

      %BlockAt("StringWriteToFlatConsString", 10000);
      %Resume("ConcurrentConcatenateStrings");

      %WaitUntilBlocked("StringWriteToFlatConsString", 10000);

      %Resume("ExternalizeStringExtensionMakeExternalOneByte");

      Atomics.wait(sab, 0, 0);

      %Resume("StringWriteToFlatConsString");

    } catch(e) {
    } finally {
      try {
        %Resume("ExternalizeStringExtensionMakeExternalOneByte");
      } catch(e) {}
      try { %Resume("ConcurrentConcatenateStrings"); } catch(e) {}
      try { %Resume("StringWriteToFlatConsString"); } catch(e) {}
      close();
    }
  }
}

var G = "A".repeat(12) + "B".repeat(12);
gc(); gc(); gc();
try { Number(G); } catch(e) {}

function f(b) {
  if (b) return G + "x";
}
%PrepareFunctionForOptimization(f);
f(true);
f(true);

let sab = new SharedArrayBuffer(4);
let sabView = new Int32Array(sab);

let worker = new Worker(workerScript, {type: 'function'});
worker.onmessage = function(msg) {
  if (msg.data === "ready") {
    %OptimizeMaglevOnNextCall(f, "concurrent");
    f(false);
    try { externalizeString(G); } catch(e) {}
    Atomics.store(sabView, 0, 1);
    Atomics.notify(sabView, 0, 1);
  }
};
worker.postMessage(sab);
