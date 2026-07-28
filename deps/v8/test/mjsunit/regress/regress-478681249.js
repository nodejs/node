// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt

function test_dead_statements_parsing() {
  function foo() { };
  var base = 1;
  assertThrows(() => {
    switch (base) {
      case foo() = 1:
      case 1: {
        foo.prototype.key_1 = function () { }
        foo.prototype.key_2 = function () { }
        return 1;
      }
    };
  }, ReferenceError);
  return 0;
}

assertEquals(test_dead_statements_parsing(), 0);
