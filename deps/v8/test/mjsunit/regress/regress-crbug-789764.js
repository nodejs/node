// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original repro (used to crash):
_v3 = ({ _v7 = (function outer() {
        for ([...[]][function inner() {}] in []) {
        }
      })} = {}) => {
};
_v3();

// Smaller repro (used to crash):
a = (b = !function outer() { for (function inner() {}.foo in []) {} }) => {};
a();
