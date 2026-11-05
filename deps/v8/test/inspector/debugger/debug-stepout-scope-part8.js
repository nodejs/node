// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Check that ScopeIterator can properly recreate the scope at every point ' +
    'when stepping through functions (part 8).');

const source = `
function nop() {}

function stress() {
  debugger;

  L: with ({x:12}) {
    break L;
  }

  with ({x: 'outer'}) {
    label: {
      with ({x: 'inner'}) {
        break label;
      }
    }
  }

  with ({x: 'outer'}) {
    label: {
      with ({x: 'inner'}) {
        break label;
      }
    }
    nop();
  }

  with ({x: 'outer'}) {
    label: {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          break label;
        }
      }
    }
  }

  with ({x: 'outer'}) {
    label: {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          break label;
        }
      }
    }
    nop();
  }

  with ({x: 'outer'}) {
    for (var i = 0; i < 3; ++i) {
      with ({x: 'inner' + i}) {
        continue;
      }
    }
  }

  with ({x: 'outer'}) {
    label: for (var i = 0; i < 3; ++i) {
      with ({x: 'middle' + i}) {
        for (var j = 0; j < 3; ++j) {
          with ({x: 'inner' + j}) {
            continue label;
          }
        }
      }
    }
  }

  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
    }
  }

  with ({x: 'outer'}) {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      nop();
    }
  }

  with ({x: 'outer'}) {
    try {
      with ({x: 'middle'}) {
        with ({x: 'inner'}) {
          throw 0;
        }
      }
    } catch (e) {
    }
  }

  try {
    with ({x: 'outer'}) {
      try {
        with ({x: 'inner'}) {
          throw 0;
        }
      } finally {
      }
    }
  } catch (e) {
  }

  try {
    with ({x: 'outer'}) {
      try {
        with ({x: 'inner'}) {
          throw 0;
        }
      } finally {
        nop();
      }
    }
  } catch (e) {
  }

  function stress1() {
    with ({x:12}) {
      return x;
    }
  }
  stress1();

  function stress2() {
    with ({x: 'outer'}) {
      with ({x: 'inner'}) {
        return x;
      }
    }
  }
  stress2();

  function stress3() {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      return e;
    }
  }
  stress3();

  function stress4() {
    try {
      with ({x: 'inner'}) {
        throw 0;
      }
    } catch (e) {
      with ({x: 'inner'}) {
        return e;
      }
    }
  }
  stress4();
}
stress();
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
