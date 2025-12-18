// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function module() {
    "use asm";
    function foo() {
      do ; while (foo ? 0 : 1) ;
      return -1 > 0 ? -1 : 0;
    }
    return foo;
}

var foo = module();
assertEquals(0, foo());
assertEquals(0, foo());
