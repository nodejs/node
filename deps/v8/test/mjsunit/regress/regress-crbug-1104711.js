// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__proto__ = {x:1};
try {
  foo();
} catch (e) {}
function foo() {
  'use strict';
  x = 42;
}
x = 42;
foo();
