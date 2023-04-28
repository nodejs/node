// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --throws

function main() {
  function g() {
    function h() {
      f;
    }
    {
      function f() {}
    }
    f;
    throw new Error();
  }
  g();
}
main();
