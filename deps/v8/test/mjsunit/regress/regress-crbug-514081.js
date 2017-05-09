// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  var __v_7 = new Worker('onmessage = function() {};');
  var e;
  var ab = new ArrayBuffer(2 * 1000 * 1000);
  try {
    __v_7.postMessage(ab);
    threw = false;
  } catch (e) {
    // postMessage failed, should be a DataCloneError message.
    assertContains('cloned', e.message);
    threw = true;
  }
  assertTrue(threw, 'Should throw when trying to serialize large message.');
}
