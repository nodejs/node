// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let count = 0
Object.prototype[Symbol.replace] = function () {
  count++
}

"".replace(0);
assertEquals(count, 1);

"".replace(0.1);
assertEquals(count, 2);
