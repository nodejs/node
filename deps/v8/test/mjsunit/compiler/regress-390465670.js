// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --assert-types

const ACTION_HANDLERS = {
  ["CALL_METHOD"]: () => { },
};

function execute(action, context) {
  for (let i = 0; i < action.inputs.length; i++) {
    let input = action.inputs[i];
  }

  let handler = ACTION_HANDLERS["CALL_METHOD"];
  context.output = handler( context.currentThis);
}

function fixup(action, currentThis) {
  let context = { currentThis: currentThis, output: undefined };
  let success = execute(action, context);
}

for (let v29 = 0; v29 < 25; v29++) {
    function F30() {
    }
    function f32() {
        const v37 = fixup({"inputs":[{"argument":{"index":0}},{"string":{"value":"set"}},{"argument":{"index":1}}]},this);
    }
    F30.toString = f32;
    class C38 extends F30 {
        [F30];
    }
}

fixup({"inputs":[{"argument":{"index":0}},{"string":{"value":"set"}},{"argument":{"index":1}}]},undefined);
