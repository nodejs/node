// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  const s = '\u8765abc';

  assertTrue(s === s);
  assertFalse(s === 'abc');
  assertFalse('abc' === s);
  assertTrue(s.slice(-3) === 'abc');
  assertTrue('abc' === s.slice(-3));
  assertTrue(s.slice(0, 1) === '\u8765');
  assertTrue('\u8765' === s.slice(0, 1));
  assertTrue(s === '' + s);
  assertTrue('' + s === s);
})();
