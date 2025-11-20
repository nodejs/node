// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for a bug where aborting a function which has a rest
// parameter didn't work correctly.
function g(...args) {
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
  x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;x;
}
