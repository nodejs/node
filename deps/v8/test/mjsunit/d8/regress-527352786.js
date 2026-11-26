// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

if (globalThis.test_mode === 'throw') {
  throw new TypeError('custom error from load');
} else if (globalThis.test_mode === 'overflow') {
  load('test/mjsunit/d8/regress-527352786.js');
} else {
  assertThrows(
      () => load('non_existent_file_527352786.js'), Error,
      /Error loading file.*non_existent_file_527352786\.js/);

  globalThis.test_mode = 'throw';
  assertThrows(
      () => load('test/mjsunit/d8/regress-527352786.js'), TypeError,
      /custom error from load/);

  globalThis.test_mode = 'overflow';
  assertThrows(() => load('test/mjsunit/d8/regress-527352786.js'), RangeError);
  delete globalThis.test_mode;
}
