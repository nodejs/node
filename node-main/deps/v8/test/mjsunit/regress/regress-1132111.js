// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Public function field with computed name
eval(`
  buggy = ((bug = new class { [0] = x => 1337.0; }) => bug);
`);

// Public method with computed name
eval(`
  buggy = ((bug = new class { [0](x) { return 1337.0}; }) => bug);
`);

// Private function field with computed name
eval(`
  buggy = ((bug = new class { #foo = x => 1337.0; }) => bug);
`);

// Private method with computed name
eval(`
  buggy = ((bug = new class { #foo(x) { return 1337.0; } }) => bug);
`);
