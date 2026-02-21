// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc

function get_template_object(x) {
  return x;
}
function foo() {
  return get_template_object``;
}
function bar() {
  return get_template_object``;
}
foo();
gc();
var cached_bar = bar();
assertNotSame(foo() === cached_bar);
assertSame(bar(), cached_bar);
