// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o;
function f() {
  "use strict";
  class C {
    static {
      o.boom;
    }
  }
  return C;
}
assertThrows(f, TypeError, /Cannot read properties of undefined/);
