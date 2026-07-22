// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test reentry of special try catch for Promises.

Debug = debug.Debug;

Debug.setBreakOnUncaughtException();
Debug.setListener(function(event, exec_state, event_data, data) { });

var p = new Promise(function(resolve, reject) { resolve(); });
var q = p.then(function() {
  new Promise(function(resolve, reject) { resolve(); });
});
