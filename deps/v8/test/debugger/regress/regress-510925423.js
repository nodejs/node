// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --fuzzing

// Trigger a ReferenceError in the inspector 'receive' callback.
// This should not crash d8 in Debug builds when --fuzzing is enabled.
globalThis.receive = function(message) {
  __v_27;
};

// Trigger a notification from the inspector.
send(JSON.stringify({
  id: 0,
  method: 'Debugger.enable',
}));
