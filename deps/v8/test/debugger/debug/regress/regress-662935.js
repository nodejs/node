// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

Debug = debug.Debug
function overflow() {
  return new Promise(function foo() { foo() });
}

function listener(event, exec_state, event_data, data) { }

Debug.setListener(listener);

assertEquals(Promise, overflow().constructor);
