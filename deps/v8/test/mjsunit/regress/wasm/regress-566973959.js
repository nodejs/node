// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kPageSize = 65536;
const memory = new WebAssembly.Memory({initial: 1, maximum: 4, shared: true});
const gsab = memory.toResizableBuffer();
memory.toFixedLengthBuffer();  // gsab is no longer the primary buffer.

const worker = new Worker(function() {
  onmessage = function({data}) {
    try {
      data.gsab.grow(data.pages * 65536);
      postMessage(data.gsab.byteLength);
    } catch (e) {
      postMessage(`${e.name}: ${e.message}`);
    }
  };
}, {type: 'function'});

worker.postMessage({mem: memory, gsab: gsab, pages: 2});
assertEquals(2 * kPageSize, worker.getMessage());
assertEquals(2 * kPageSize, gsab.byteLength);

worker.postMessage({gsab: gsab, pages: 3});
assertEquals(3 * kPageSize, worker.getMessage());
assertEquals(3 * kPageSize, gsab.byteLength);
