// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function receive(message) {
  // Because location is missing, do not start the profiler
  assertNotEquals("Profiler.consoleProfileStarted", JSON.parse(message).method);
}

send(JSON.stringify({id: 1, method: "Profiler.enable"}));
send(JSON.stringify({id: 2, method: "Debugger.enable"}));

// Bind console.profile to console to ensure 'this' is correct
const profile = console.profile.bind(console);

// Schedule execution directly from event loop to have 0 JS frames
setTimeout(profile, 0);
