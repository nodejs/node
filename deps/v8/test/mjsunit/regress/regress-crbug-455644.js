// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function f() {
  do { return 23; } while(false);
  with (0) {
    try {
      return 42;
    } finally {}
  }
})();
