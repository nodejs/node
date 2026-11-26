// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-stack-trace-frames --throws

Error.prepareStackTrace = (e, frames) => {
  for (let frame of frames) {
    let fn = frame.getFunction();
    if (fn) fn();
  }
  return frames;
};

import('data:text/javascript,throw 123;');
