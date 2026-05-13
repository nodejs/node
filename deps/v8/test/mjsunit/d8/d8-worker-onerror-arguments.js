// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test arguments passed to onerror in a worker using {type: 'function'}.

const w = new Worker(function() {
  postMessage('ready');
  globalThis.onerror = function(msg, source, lineno, colno, ex) {
    if (typeof msg !== 'string') {
      postMessage('FAIL: msg is not a string, it is ' + typeof msg);
      return true;
    }
    if (typeof source !== 'string') {
      postMessage('FAIL: source is not a string, it is ' + typeof source);
      return true;
    }
    if (typeof lineno !== 'number' || lineno <= 0) {
      postMessage('FAIL: lineno is not a positive number, it is ' + lineno);
      return true;
    }
    if (typeof colno !== 'number' || colno <= 0) {
      postMessage('FAIL: colno is not a positive number, it is ' + colno);
      return true;
    }
    if (typeof ex !== 'object') {
      postMessage('FAIL: ex is not an object, it is ' + typeof ex);
      return true;
    }
    if (msg.indexOf('foo') === -1) {
      postMessage('FAIL: msg does not contain "foo", it is "' + msg + '"');
      return true;
    }
    if (ex.message !== 'foo') {
      postMessage('FAIL: ex.message is not "foo", it is "' + ex.message + '"');
      return true;
    }
    postMessage('PASS');
    return true; // Suppress
  };
  throw new Error('foo');
}, {type: 'function'});

w.onmessage = function(e) {
  const msg = e.data;
  if (msg === 'ready') return;
  if (msg === 'PASS') {
    w.terminate();
    return;
  }
  fail('Worker failed with: ' + JSON.stringify(msg));
};
