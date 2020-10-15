// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function runTestWithWorker(script, expected_messages) {
  const w = new Worker(script, {type : 'string'});
  w.postMessage('start');
  let i = 0;
  while (i < expected_messages.length) {
    const m = w.getMessage();
    assertEquals(expected_messages[i], m);
    ++i;
  }
  w.terminate();
}
