// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbo-deoptimization

assertThrows(function() {
  [0].every(function(){ Object.seal((new Int8Array())); });
})

assertThrows(function() {
  "use strict";
  const v = 42;
  v += 1;
});
