// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(Number.MIN_VALUE * 2 > 0);

let flushed = new Worker(function() {
  postMessage(Number.MIN_VALUE * 2 > 0);
  postMessage(Number.MIN_VALUE * 2 == 0);
}, {
  type: "function",
  flushDenormals: true
});
assertFalse(flushed.getMessage());
assertTrue(flushed.getMessage());

let nonFlushed = new Worker(function() {
  postMessage(Number.MIN_VALUE * 2 > 0);
}, {
  type: "function",
  flushDenormals: false
});
assertTrue(nonFlushed.getMessage());

let defaultFlush = new Worker(function() {
  postMessage(Number.MIN_VALUE * 2 > 0);
}, {
  type: "function"
});
assertTrue(defaultFlush.getMessage());

d8.test.setFlushDenormals(true);

let defaultFlushWithFlush = new Worker(function() {
  postMessage(Number.MIN_VALUE * 2 > 0);
  postMessage(Number.MIN_VALUE * 2 == 0);
}, {
  type: "function"
});
assertFalse(defaultFlushWithFlush.getMessage());
assertTrue(defaultFlushWithFlush.getMessage());

d8.test.setFlushDenormals(false);
