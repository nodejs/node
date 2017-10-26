// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-harmony-restrict-constructor-return

this.FLAG_harmony_restrict_constructor_return = false;
try {
  load('mjsunit/compiler/constructor-inlining.js');
} catch(e) {
  load('test/mjsunit/compiler/constructor-inlining.js');
}
