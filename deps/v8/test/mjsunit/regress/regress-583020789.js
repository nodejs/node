// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const literal = '9'.repeat(500000) + 'n';

const fn = eval(`
  let x;
  () => {
    if (false) x[${literal}];
    d8.terminate();
    (${literal})();
  }
`);

assertThrows(fn, TypeError);
