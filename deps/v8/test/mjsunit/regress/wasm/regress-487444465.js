// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const worker = new Worker(function() {
  onmessage = () => postMessage('ok');
}, {type: 'function'});

const memory = new WebAssembly.Memory(
    {initial: 1n, maximum: 47n, shared: true, address: 'i64'});

const view = new Uint32Array(memory.toResizableBuffer());

worker.postMessage(view);
// Note: If this hangs, it might be due to a failure on deserialization. Run
// with --print-all-exceptions to confirm.
worker.getMessage();
