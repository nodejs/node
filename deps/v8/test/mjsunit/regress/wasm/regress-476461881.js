// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const memory =
    new WebAssembly.Memory({initial: 1, maximum: 1 << 16, shared: true});
const worker = new Worker(
    () => {this.onmessage = m => this.postMessage(m.data)}, {type: 'function'});

let a = memory.toResizableBuffer();
worker.onmessage = () => {};
worker.postMessage(memory);

memory.toFixedLengthBuffer();
let m = worker.getMessage();

assertInstanceof(m, WebAssembly.Memory);
assertInstanceof(m.buffer, SharedArrayBuffer);
assertSame(undefined, m.buffer.resizable);

worker.terminate();
