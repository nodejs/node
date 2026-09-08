// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-inspector --turbofan --no-maglev

let msgId = 1;
function cmd(method, params) {
  return JSON.stringify({id: msgId++, method: method, params: params || {}});
}

function receive(msg) {
  let obj = JSON.parse(msg);
  if (obj.method === "Debugger.paused") {
    send(cmd("Debugger.resume"));
  }
}

let top_frame_status_after_break = -1;
function check_deopt() {
  eval("");
  top_frame_status_after_break = %GetOptimizationStatus(osr_top);
}
%NeverOptimizeFunction(check_deopt);

function osr_top() {
  for (let i = 0; i < 20; i++) {
    if (i === 10) {
      %OptimizeOsr();
    }
  }
  debugger;
  check_deopt();
}

send(cmd("Debugger.enable"));
%PrepareFunctionForOptimization(osr_top);
osr_top();

// kTopmostFrameIsTurboFanned is bit 11 (1 << 11 = 2048) of GetOptimizationStatus
const kTopmostFrameIsTurboFanned = 1 << 11;
const isTopFrameTurboFanned = (top_frame_status_after_break & kTopmostFrameIsTurboFanned) !== 0;

assertFalse(isTopFrameTurboFanned, "OSR topmost frame should be deoptimized after debugger break");
