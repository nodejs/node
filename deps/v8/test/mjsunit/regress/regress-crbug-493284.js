// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --invoke-weak-callbacks --omit-quit --no-test

if (this.Intl) {
  var coll = new Intl.Collator();
  assertEquals(-1, coll.compare('a', 'c'));
}
