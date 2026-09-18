// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function Regress522454846() {
  function workerCode() {
    var mem;
    onmessage = function(e) {
      var msg = e.data;
      if (msg.memory) {
        mem = msg.memory;
      } else if (msg.check) {
        postMessage({size: mem.buffer.byteLength});
      }
    };
  }

  var worker = new Worker(workerCode, {type: 'function'});

  var initial_pages = 1;
  var grow_pages = 1;
  var memory = new WebAssembly.Memory({
    initial: initial_pages,
    maximum: 10,
    shared: true
  });

  worker.postMessage({memory: memory});

  // Grow memory immediately after posting.
  memory.grow(grow_pages);

  var expected_size = (initial_pages + grow_pages) * 65536;
  assertEquals(expected_size, memory.buffer.byteLength);

  worker.postMessage({check: true});

  var worker_data = worker.getMessage();
  var worker_size = worker_data.size;

  assertEquals(expected_size, worker_size, "Worker saw stale memory size!");

  worker.terminate();
})();
