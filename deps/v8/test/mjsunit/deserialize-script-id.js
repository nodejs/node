// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code
// Test that script ids are unique and we found the correct ones.

var scripts = %DebugGetLoadedScripts();
scripts.sort(function(a, b) { return a.id - b.id; });
var user_script_count = 0;
scripts.reduce(function(prev, cur) {
  assertTrue(prev === undefined || prev.id != cur.id);
  if (cur.type == 2) user_script_count++;
});

// Found mjsunit.js and this script.
assertEquals(2, user_script_count);
