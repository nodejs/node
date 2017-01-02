// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-debug-as debug

Debug = debug.Debug;
Debug.setListener(function() {});
Debug.setBreakOnException();

try {
  try {
    %DebugPushPromise(new Promise(function() {}));
  } catch (e) {
  }
  throw new Error();
} catch (e) {
}

Debug.setListener(null);
