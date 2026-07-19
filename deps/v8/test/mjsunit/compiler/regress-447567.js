// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(function () {
  Object.freeze(new Int8Array(1))
});

assertThrows(function() {
  "use strict";
  const v = 42;
  v += 1;
});
