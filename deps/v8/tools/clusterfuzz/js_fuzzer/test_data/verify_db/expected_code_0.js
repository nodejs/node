// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: resources/cross_over_template_2.js
let __v_0 = 0;
let __v_1 = 0;
class __c_0 {}
class __c_1 extends __c_0 {
  constructor() {
    console.log(42);
    /* CrossOverMutator: Crossover from foo */
    super();
  }
}
