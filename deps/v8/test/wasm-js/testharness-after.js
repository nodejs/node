// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Uses lastPromise defined in testharness.js

assertPromiseResult(lastPromise, _ => {
  if (failures.length > 0) {
    let message = 'Some tests FAILED:\n';
    for (const failure of failures) {
      message += `  ${failure}\n`;
    }

    failWithMessage(message);
  }
});
