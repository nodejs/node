// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

let err = {};
(function boom() {
  Error.captureStackTrace(err);
})();
Promise.reject(err);

let stack = err.stack;
gc();
