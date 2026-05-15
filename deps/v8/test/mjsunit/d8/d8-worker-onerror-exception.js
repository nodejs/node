// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test onerror in a worker where onerror throws an exception.

const w = new Worker(function() {
  globalThis.onerror = function(msg, source, lineno, colno, ex) {
    throw new Error('error in onerror');
  };

  onmessage = function(e) {
    const msg = e.data;
    if (msg === 'ping') {
      postMessage('pong');
    }
  };

  // Throw an error to trigger onerror
  throw new Error('foo');
}, {type: 'function'});

w.onmessage = function(e) {
  const msg = e.data;
  if (msg === 'pong') {
    w.terminate();
  }
};

// Use a ping message to verify the worker is still responsive after an error
// and its resulting onerror handler exception.
w.postMessage('ping');
