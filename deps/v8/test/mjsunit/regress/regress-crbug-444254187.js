// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

class Outer {
  static [
    class Inner {
        static accessor [
          (() => {
            return "inner_prop";
          })()
        ] = 123;

        static getName() { return "outer_prop"; }

    }
  ]
  = 99;
}
