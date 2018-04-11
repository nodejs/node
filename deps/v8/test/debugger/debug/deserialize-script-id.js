// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code
// Test that script ids are unique and we found the correct ones.

var Debug = debug.Debug;
Debug.setListener(function(){});

var scripts = %DebugGetLoadedScripts();
scripts.sort(function(a, b) { return a.id - b.id; });
scripts.reduce(function(prev, cur) {
  assertTrue(prev === undefined || prev.id != cur.id);
});

Debug.setListener(null);
