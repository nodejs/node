// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => new Intl.v8BreakIterator('en', null));
assertDoesNotThrow(() => new Intl.v8BreakIterator('en', undefined));

for (let key of [false, true, "foo", Symbol, 1]) {
  assertDoesNotThrow(() => new Intl.v8BreakIterator('en', key));
}

assertDoesNotThrow(() => new Intl.v8BreakIterator('en', {}));
assertDoesNotThrow(() => new Intl.v8BreakIterator('en', new Proxy({}, {})));
