// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests ScopeIterator scope recreation while stepping (part 1).');

const source = `
function test1() {
  debugger;
  with ({x:1}) {
    x = 2;
  }
}
test1();

function test2() {
  if (true) {
    with ({}) {
      debugger;
    }
  } else {
    with ({}) {
      return 10;
    }
  }
}
test2();

function test3() {
  if (true) {
    debugger;
  } else {
    with ({}) {
      return 10;
    }
  }
}
test3();

function test4() {
  debugger;
  with ({x:1}) x = 1
}
test4();

function test5() {
  debugger;
  var dummy = 1;
  with ({}) {
    with ({}) {
      dummy = 2;
    }
  }
  dummy = 3;
}
test5();

function test6() {
  debugger;
  try {
    throw 'stuff';
  } catch (e) {
    e = 1;
  }
}
test6();

function test7() {
  debugger;
  function foo() {}
}
test7();

function test8() {
  debugger;
  (function foo() {})();
}
test8();

function test10() {
  debugger;
  with ({}) {
    return 10;
  }
}
test10();

function test11() {
  debugger;
  try {
    throw 'stuff';
  } catch (e) {
    return 10;
  }
}
test11();

var prefixes = [
    "debugger; ",
    "if (false) { try { throw 0; } catch(x) { return x; } }; debugger; " ];

var anon = 12;
for (var i = 0; i < prefixes.length; ++i) {
  var pre = prefixes[i];
  Function(pre + "return 42")();
  Function(pre + "return 42 ")();
  Function(pre + "return 42;")();
  Function(pre + "return 42; ")();
  Function(pre + "return anon")();
  Function(pre + "return anon ")();
  Function(pre + "return anon;")();
  Function(pre + "return anon; ")();
}

try {
  with({}) {
    debugger;
    eval("{}$%:^");
  }
} catch(e) {
  nop();
}

function nop() {}

with ({}) { debugger; }
`;

(async function test() {
  await Protocol.Debugger.enable();

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    pauseCount++;
    for (const scope of callFrames[0].scopeChain) {
      const response = await Protocol.Runtime.getProperties(
          {objectId: scope.object.objectId, ownProperties: true});
      if (response.error) {
        InspectorTest.log(
            `Error getting properties for scope ${scope.type}: ` +
            `${response.error.message}`);
      }
    }
    await Protocol.Debugger.stepInto();
  });

  await Protocol.Runtime.evaluate({expression: source});
  InspectorTest.log(`Total pauses: ${pauseCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
