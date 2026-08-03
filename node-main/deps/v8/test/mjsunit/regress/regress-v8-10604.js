// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

(async function test() {
  try {
    import('does_not_exist.mjs');
    assertUnreachable();
  } catch {};

  try {
    await eval("import('does_not_exist.mjs')");
    assertUnreachable();
  } catch {};

  try {
    await Realm.eval(Realm.create(), "import('does_not_exist.mjs')");
    assertUnreachable();
  } catch {};
})();
