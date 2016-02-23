// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug;

assertThrows("Debug.scripts()");
Debug.setListener(function(){});

assertDoesNotThrow("Debug.scripts()");
Debug.setListener(null);
