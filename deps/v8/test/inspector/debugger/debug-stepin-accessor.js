// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into property accessor calls (getters and setters).');

const source = `
var c = {
  name: 'name ',
  get getter1() {
    return this.name;  // getter 1
  },
  get getter2() {
    return {  // getter 2
     'a': c.name
    };
  },
  set setter1(n) {
    this.name = n;  // setter 1
  }
};

c.__defineGetter__('y', function getterY() {
  return this.name;  // getter y
});

c.__defineGetter__(3, function getter3() {
  return this.name;  // getter 3
});

c.__defineSetter__('y', function setterY(n) {
  this.name = n;  // setter y
});

c.__defineSetter__(3, function setter3(n) {
  this.name = n;  // setter 3
});

var d = {
  'c': c,
};

function testGetter1_1() {
  debugger;
  var x = c.getter1;
}

function testGetter1_2() {
  debugger;
  var x = c['getter1'];
}

function testGetter1_3() {
  for (var i = 1; i < 2; i++) {
    debugger;
    var x = c['getter' + i];
  }
}

function testGetter1_4() {
  debugger;
  var x = d.c.getter1;
}

function testGetter1_5() {
  for (var i = 2; i != 1; i--);
  debugger;
  var x = d.c['getter' + i];
}

function testGetter2_1() {
  for (var i = 2; i != 1; i--);
  debugger;
  var t = d.c.getter2.name;
}

function testGetterY_1() {
  debugger;
  var t = d.c.y;
}

function testIndexedGetter3_1() {
  debugger;
  var r = d.c[3];
}

function testSetterY_1() {
  debugger;
  d.c.y = 'www';
}

function testIndexedSetter3_1() {
  var i = 3;
  debugger;
  d.c[3] = 'www';
}

function testSetter1_1() {
  debugger;
  d.c.setter1 = 'aa';
}

function testSetter1_2() {
  debugger;
  d.c['setter1'] = 'bb';
}

function testSetter1_3() {
  for (var i = 2; i != 1; i--);
  debugger;
  d.c['setter' + i] = i;
}

var e = {
  name: 'e'
};
e.__proto__ = c;

function testProtoGetter1_1() {
  debugger;
  var x = e.getter1;
}

function testProtoSetter1_1() {
  debugger;
  e.setter1 = 'aa';
}

function testProtoIndexedGetter3_1() {
  debugger;
  var x = e[3];
}

function testProtoIndexedSetter3_1() {
  debugger;
  e[3] = 'new val';
}

function testProtoSetter1_2() {
  for (var i = 2; i != 1; i--);
  debugger;
  e['setter' + i] = 'aa';
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

const sourceLines = source.split('\n');

(async function test() {
  await Protocol.Debugger.enable();

  const testFunctions = [
    'testGetter1_1',
    'testGetter1_2',
    'testGetter1_3',
    'testGetter1_4',
    'testGetter1_5',
    'testGetter2_1',
    'testGetterY_1',
    'testIndexedGetter3_1',
    'testSetterY_1',
    'testIndexedSetter3_1',
    'testSetter1_1',
    'testSetter1_2',
    'testSetter1_3',
    'testProtoGetter1_1',
    'testProtoSetter1_1',
    'testProtoIndexedGetter3_1',
    'testProtoIndexedSetter3_1',
    'testProtoSetter1_2',
  ];

  for (const fn of testFunctions) {
    let state = 1;
    let insideTarget = false;

    const onPaused = async ({params: {callFrames}}) => {
      const topFrame = callFrames[0];
      if (state === 1) {
        // Paused at `debugger;`
        state = 2;
        await Protocol.Debugger.stepInto();
      } else if (state === 2) {
        // Paused at accessor access statement
        state = 3;
        await Protocol.Debugger.stepInto();
      } else if (state === 3) {
        // Paused inside accessor
        state = 4;
        insideTarget = true;
        const lineText = sourceLines[topFrame.location.lineNumber].trim();
        InspectorTest.log(`${fn}: ${topFrame.functionName} at '${lineText}'`);
        await Protocol.Debugger.resume();
      } else {
        await Protocol.Debugger.resume();
      }
    };

    Protocol.Debugger.onPaused(onPaused);
    await Protocol.Runtime.evaluate({expression: `${fn}();`});
    Protocol.Debugger.onPaused(null);

    if (!insideTarget || state !== 4) {
      InspectorTest.log(
          `FAIL: ${fn} did not pause inside accessor (state=${state})`);
    }
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
