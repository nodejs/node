// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

function foo() {
  'use strict';
  x = 42;
}

__proto__ = {x: 1};

assertThrows(foo);
assertThrows(foo);
