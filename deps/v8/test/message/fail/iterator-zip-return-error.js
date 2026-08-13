// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-joint-iteration

const it = {
  next() { return { value: 1, done: false }; },
  return() {
    throw new Error("original_error_location");
  }
};

const zipped = Iterator.zip([it]);
zipped.next();
zipped.return();
