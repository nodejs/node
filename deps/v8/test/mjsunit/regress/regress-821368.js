// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const worker = new Worker("onmessage = function(){}", {type: 'string'});
const buffer = new ArrayBuffer();
worker.postMessage(buffer, [buffer]);
try {
  worker.postMessage(buffer, [buffer]);
} catch (e) {
  if (e != "ArrayBuffer could not be transferred") {
    throw e;
  }
}
