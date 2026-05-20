// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";
  function f() {
   var i = (140737463189505);
   do {
    i = i + i | 0;
    x = undefined + i | 0;
   } while (!i);
  }
  return { f: f };
}

Module().f();
