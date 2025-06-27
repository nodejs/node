// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code
// Test that script ids are unique and we found the correct ones.

var Debug = debug.Debug;
Debug.setListener(function(){});

var ids = %DebugGetLoadedScriptIds();
ids.sort((a, b) => a - b);
ids.reduce((prev, cur) => assertTrue(prev === undefined || prev != cur));

Debug.setListener(null);
