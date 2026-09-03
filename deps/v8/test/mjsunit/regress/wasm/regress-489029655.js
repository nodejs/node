// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const memory = new WebAssembly.Memory({initial: 1, maximum: 2, shared: true});
const gsab = memory.toResizableBuffer();
const sab = memory.toFixedLengthBuffer();
const worker = new Worker(
    () => {this.onmessage = m => this.postMessage(m.data.gsab)},
    {type: 'function'});
worker.postMessage({mem: memory, gsab: gsab});
