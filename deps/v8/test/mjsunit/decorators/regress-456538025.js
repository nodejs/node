// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-decorators

((arg = (function wrapper() {
    class C {
      accessor[Symbol.for('computed')] = function f() {};
    }
    return C;
  })()) => {})();
