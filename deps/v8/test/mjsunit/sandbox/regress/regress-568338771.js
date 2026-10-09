// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --sandbox-testing --allow-natives-syntax

function receive(message) {}

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));

function pad(x) {
  return x;
}
%NeverOptimizeFunction(pad);

const donor = (0, eval)(
    '(function donor(a, b) { let x = a; return b; })\n' +
    '//# sourceURL=donor.js');
donor(1, 2);

send(JSON.stringify({
  id: 2,
  method: 'Debugger.setBreakpointByUrl',
  params: {url: 'donor.js', lineNumber: 0, columnNumber: 24}
}));

function victim(a, b) {
  pad(a);
  pad(a);
  pad(a);
  pad(a);
  return b + 1;
}

%PrepareFunctionForOptimization(victim);
victim(1, 2);
%OptimizeFunctionOnNextCall(victim);
victim(1, 2);

const donor_sfi =
    Sandbox.dereferenceTaggedPointerField(donor, 'shared_function_info');
const victim_sfi =
    Sandbox.dereferenceTaggedPointerField(victim, 'shared_function_info');
const donor_id = Sandbox.readObjectField(donor_sfi, 'unique_id', 32);
Sandbox.corruptObjectField(victim_sfi, 'unique_id', donor_id, 32);

victim(1, {});
