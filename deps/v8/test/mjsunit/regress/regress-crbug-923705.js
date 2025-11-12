// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

function __f_5() {
  function __f_1() {
    function __f_0() {
      ({y = eval()}) => assertEquals()();
    }
  }
}

__f_5();
