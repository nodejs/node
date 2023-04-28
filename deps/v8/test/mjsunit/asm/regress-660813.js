// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module() {
  "use asm";
  const i = 0xffffffff;
  function foo() {
    return i;
  }
}
Module();
