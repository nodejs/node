// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector --turbofan --no-maglev

let msgId = 1;
function cmd(method, params) { return JSON.stringify({id: msgId++, method: method, params: params || {}}); }

let paused_locations = [];

function receive(msg) {
  let obj = JSON.parse(msg);
  if (obj.method === "Debugger.paused") {
    let fnName = obj.params.callFrames[0].functionName;
    paused_locations.push(fnName);
    if (fnName === "callee") {
      send(cmd("Debugger.stepInto"));
    } else {
      send(cmd("Debugger.resume"));
    }
  }
}

function target() {
  return 1;
}

function callee() {
  eval("");
  debugger;
}
%NeverOptimizeFunction(callee);

eval(`
function osr_caller(osr) {
  for (let i = 0; i < 20; i++) {
    if (osr && i === 10) %OptimizeOsr();
  }
  callee();
  target();
}
//# sourceURL=osr_caller.js
`);

// Gather feedback for inlining
%PrepareFunctionForOptimization(target);
callee();
target();
callee();
target();

%PrepareFunctionForOptimization(osr_caller);
osr_caller(false);
osr_caller(false);

send(cmd("Debugger.enable"));
// Blackbox osr_caller so FloodWithOneShot is skipped, exposing the bug in PrepareStep
send(cmd("Debugger.setBlackboxPatterns", { patterns: ["osr_caller\\.js"] }));

paused_locations = [];
osr_caller(true);

assertTrue(paused_locations.includes("target"), "BUG: target was not stepped into across blackboxed OSR caller");
