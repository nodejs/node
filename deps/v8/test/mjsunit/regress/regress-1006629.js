// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const workerScript = `
  onmessage = function() {
  };`;
const worker = new Worker(workerScript, {type: 'string'});
const i32a = new Int32Array( new SharedArrayBuffer() );
worker.postMessage([i32a.buffer]);
