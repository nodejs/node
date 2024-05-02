// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function get() {
  // Update the descriptor array now shared between the Foo map and the
  // (Foo + c) map.
  o1.c = 10;
  // Change the type of the field on the new descriptor array in-place to
  // Tagged. If Object.assign has a cached descriptor array, then it will point
  // to the old Foo map's descriptors, which still have .b as Double.
  o2.b = "string";
  return 1;
}

function Foo() {
  Object.defineProperty(this, "a", {get, enumerable: true});
  // Initialise Foo.b to have Double representation.
  this.b = 1.5;
}

var o1 = new Foo();
var o2 = new Foo();
var target = {};
Object.assign(target, o2);
// Make sure that target has the right representation after assignment.
assertEquals(target.b, "string");
