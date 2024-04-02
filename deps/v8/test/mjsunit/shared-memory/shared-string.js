// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --allow-natives-syntax

if (this.Worker) {

(function TestSharedStringPostMessage() {
  let workerScript =
      `postMessage("started");
       onmessage = function(str) {
         if (!%IsSharedString(str)) {
           throw new Error("str isn't shared");
         }
         postMessage(str);
       };`;

  let worker = new Worker(workerScript, { type: 'string' });
  let started = worker.getMessage();
  assertTrue(%IsSharedString(started));
  assertEquals("started", started);

  // The string literal appears in source and is internalized, so should
  // already be shared.
  let str_to_send = 'foo';
  assertTrue(%IsSharedString(str_to_send));
  worker.postMessage(str_to_send);
  let str_received = worker.getMessage();
  assertTrue(%IsSharedString(str_received));
  // Object.is and === won't check pointer equality of Strings.
  assertTrue(%IsSameHeapObject(str_to_send, str_received));

  worker.terminate();
})();

}
