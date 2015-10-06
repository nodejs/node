// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  var __v_7 = new Worker('onmessage = function() {};');
  try {
    var ab = new ArrayBuffer(2147483648);
    // If creating the ArrayBuffer succeeded, then postMessage should fail.
    assertThrows(function() { __v_7.postMessage(ab); });
  } catch (e) {
    // Creating the ArrayBuffer failed.
    assertInstanceof(e, RangeError);
  }
}
