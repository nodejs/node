// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  // Generates a forward branch that puts 200 in the constant pool.
  var i = 0;
  if (i) {
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
    i = 0; i = 0; i = 0; i = 0; i = 0; i = 0;
  }
  // Emit a 200 literal which also ends up in the constant pool.
  var j = 0.2e3;
}
foo();
