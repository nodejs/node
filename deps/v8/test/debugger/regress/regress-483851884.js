// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

globalThis.receive = function(msg) {}

var s = "";
try {
  s = "a".repeat(256 * 1024 * 1024);
} catch(e) {
  quit(1);
}

// 10 copies => ~2.5GB JSON payload
var expr = "[s, s, s, s, s, s, s, s, s, s]";

// Enable Runtime domain via d8's internal send
send(JSON.stringify({id: 1, method: "Runtime.enable"}));

// Trigger huge JSON serialization
send(JSON.stringify({
  id: 2,
  method: "Runtime.evaluate",
  params: {
    expression: expr,
    returnByValue: true
  }
}));
