// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-sparkplug

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Sparkplug: result pair of FindNonDefaultConstructorOrConstruct is lost ' +
    'when the debugger discards the baseline frame');

contextGroup.addScript(`
class A {}
const nop = () => {};

let seenThis;
class Observe extends A {
  constructor(g, v) {
    g(0, 0, true, v);       // leaves the temporaries r6 = true, r7 = v behind
    super();                // FindNonDefaultConstructorOrConstruct -> r6, r7
    seenThis = this;
  }
}

class Brand extends A {
  #brand() {}               // super() is followed by DefineKeyedOwnProperty(this, #brand)
  constructor(g, v) {
    g(0, 0, true, v);
    super();
  }
}

const P = new Proxy(function () {}, {
  get(t, k, r) {
    if (k === 'prototype') {
      console.log('in trap');
      // Debugger.enable is processed here, i.e. while the constructor's
      // baseline frame is suspended inside the builtin call.
      inspector.runNestedMessageLoop();
      debugger;
    }
    return Reflect.get(t, k, r);
  }
});

function attack(Target, v) {
  seenThis = undefined;
  try {
    const o = Reflect.construct(Target, [nop, v], P);
    return 'constructed ' + Object.prototype.toString.call(o) + ' (correct)';
  } catch (e) {
    return 'BUG: ' + e + '; typeof this after super() = ' + typeof seenThis +
           ', this = ' + seenThis;
  }
}
`);

async function run(expression) {
  InspectorTest.log('\nRunning ' + expression);
  const inTrap = Protocol.Runtime.onceConsoleAPICalled();
  const result = Protocol.Runtime.evaluate({expression});
  await inTrap;
  Protocol.Debugger.enable();                    // handled inside the trap
  Protocol.Runtime.runIfWaitingForDebugger();    // leaves the nested loop
  await Protocol.Debugger.oncePaused();          // `debugger;` in the trap
  InspectorTest.log('paused in trap, stepping out');
  Protocol.Debugger.stepOut();
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused in ' + callFrames[0].functionName + ', resuming');
  Protocol.Debugger.resume();
  InspectorTest.log((await result).result.result.value);
  await Protocol.Debugger.disable();
}

(async () => {
  await Protocol.Runtime.enable();
  await run('attack(Observe, 0x20000000)');
  await run('attack(Brand, 0x20000000)');        // dereferences the Smi
  InspectorTest.completeTest();
})();
