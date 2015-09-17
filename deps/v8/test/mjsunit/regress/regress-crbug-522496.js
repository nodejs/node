// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  var worker = new Worker("onmessage = function(){}");
  var buf = new ArrayBuffer();
  worker.postMessage(buf, [buf]);
}
