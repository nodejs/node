// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-opt --no-stress-validate-asm --no-suppress-asm-messages

function Module(stdlib, foreign, heap) {
  "use asm"
  var x = +stdlib.x;
}
Module(this, { x:0 });
