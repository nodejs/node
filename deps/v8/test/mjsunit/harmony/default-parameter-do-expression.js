// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-do-expressions --nolazy

function hoist_unique_do_var() {
  var f = (y = do { var unique = 3 }) => unique;
  assertEquals(3, f());
  assertThrows(() => unique, ReferenceError);
}
hoist_unique_do_var();

function hoist_duplicate_do_var() {
  var duplicate = 100;
  var f = (y = do { var duplicate = 3 }) => duplicate;
  assertEquals(3, f());
  // TODO(verwaest): The {duplicate} declarations were invalidly merged.
  assertEquals(3, duplicate);
}
hoist_duplicate_do_var();
