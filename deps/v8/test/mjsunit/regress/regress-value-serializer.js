// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-immutable-arraybuffer

const ab = new ArrayBuffer(0);
const immutable = ab.transferToImmutable();

const w = new Worker('onmessage = function(e) {};', {type: 'string'});
w.postMessage(immutable);
w.terminate();
