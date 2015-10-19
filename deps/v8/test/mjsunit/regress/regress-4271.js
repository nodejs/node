// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  // Throw rather than overflow internal field index
  assertThrows(function() {
    Worker.prototype.terminate();
  });

  assertThrows(function() {
    Worker.prototype.getMessage();
  });

  assertThrows(function() {
    Worker.prototype.postMessage({});
  });

  // Don't throw for real worker
  var worker = new Worker('');
  worker.getMessage();
  worker.postMessage({});
  worker.terminate();
}
