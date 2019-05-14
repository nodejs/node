// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => new Intl.NumberFormat('en', null));
assertDoesNotThrow(() => new Intl.NumberFormat('en', undefined));

for (let key of [false, true, "foo", Symbol, 1]) {
  assertDoesNotThrow(() => new Intl.NumberFormat('en', key));
}

assertDoesNotThrow(() => new Intl.NumberFormat('en', {}));
assertDoesNotThrow(() => new Intl.NumberFormat('en', new Proxy({}, {})));
