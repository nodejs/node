// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  var __v_7 = new Worker('onmessage = function() {};');
  var e;
  try {
    var ab = new ArrayBuffer(2147483648);
    try {
      __v_7.postMessage(ab);
    } catch (e) {
      // postMessage failed, should be a DataCloneError message.
      assertContains('cloned', e.message);
    }
  } catch (e) {
    // Creating the ArrayBuffer failed.
    assertInstanceof(e, RangeError);
  }
}
