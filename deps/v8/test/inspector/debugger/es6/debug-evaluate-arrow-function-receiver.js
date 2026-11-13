// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that debug-evaluate can find the correct this value for ' +
    'arrow functions.');

const scriptSource = `
// Context-allocated receiver.
function f() {
  debugger;                          // foo
  return () => {
    debugger;                        // foo
    with ({}) {
      return () => {
        debugger;                    // foo
        try {
          throw new Error();
        } catch (e) {
          return () => {
            (() => this);            // bind this.
            debugger;                // foo
            return () => {
              debugger;              // undefined
              return g.call("goo");  // undefined
            }
          };
        }
      };
    }
  };
}

// Stack-allocated receiver.
function g() {
  debugger;                        // goo
  return () => {
    debugger;                      // undefined
    with ({}) {
      return () => {
        debugger;                  // undefined
        try {
          throw new Error();
        } catch (e) {
          return () => {
            debugger;              // undefined
            return f.call("foo");  // undefined
          };
        }
      };
    }
  };
}

function runTests() {
  var h = f.call("foo");
  for (var i = 0; i < 20; i++) h = h();
  var h = g.call("goo");
  for (var i = 0; i < 20; i++) h = h();

  function x() {
    (() => this);      // bind this.
    function y() {
      (() => {
        (() => this);  // bind this.
        debugger;      // Y
       })();           // Y
    }
    y.call("Y");       // X
  }
  x.call("X");

  function u() {
    (() => this);
    function v() {
      (() => {
        debugger;      // undefined
       })();           // V
    }
    v.call("V");       // U
  }
  u.call("U");

  (() => {
    (() => this);
    debugger;          // [object global]
  })();
}
`;

contextGroup.addScript(scriptSource, 0, 0, 'test.js');
const scriptLines = scriptSource.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  let breakCount = 0;
  let mismatches = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    breakCount++;
    for (let i = 0; i < callFrames.length - 1; i++) {
      const frame = callFrames[i];
      const lineText = scriptLines[frame.location.lineNumber];
      if (!lineText) continue;
      const match = lineText.match(/\/\/ (.*$)/);
      if (!match) continue;
      const expected = match[1];
      const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: frame.callFrameId, expression: '\'\' + this'});
      const actual = result.value;
      InspectorTest.log(`${expected} ${actual} ${lineText.trim()}`);
      if (String(expected) !== String(actual)) {
        mismatches++;
      }
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'runTests()'});
  InspectorTest.log(`Total breaks: ${breakCount}`);
  InspectorTest.log(`Mismatches: ${mismatches}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
