// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into Set and Map forEach callbacks.');

const source = `
var s = new Set();
s.add(1);
s.add(2);
s.add(3);

var m = new Map();
m.set('foo', 1);
m.set('bar', 2);
m.set('baz', 3);

function cb_set(num) {
  var msg = "element " + num; // B2 B5 B8
  return true                 // B3 B6 B9
  ;                           // B4 B7 B10
}

function cb_map(key, val) {
  var msg = "key " + key + ", value " + val; // B2 B5 B8
  return true                                // B3 B6 B9
  ;                                          // B4 B7 B10
}

function testSet() {
  debugger;          // B0
  s.forEach(cb_set); // B1
}                    // B11

function testMap() {
  debugger;          // B0
  m.forEach(cb_map); // B1
}                    // B11

function cb_set_2(num) {
  var msg = "element " + num; // B3 B6 B9  B15 B18 B21 B27 B30 B33
  return true                 // B4 B7 B10 B16 B19 B22 B28 B31 B34
  ;                           // B5 B8 B11 B17 B20 B23 B29 B32 B35
}

function cb_map_2(k, v) {
  var msg = \`key \${k}, value \${v}\`; // B3 B6 B9  B15 B18 B21 B27 B30 B33
  return true                       // B4 B7 B10 B16 B19 B22 B28 B31 B34
  ;                                 // B5 B8 B11 B17 B20 B23 B29 B32 B35
}

function cb_set_foreach(num) {
  s.forEach(cb_set_2);        // B2  B14 B26
  var back = "back.";         // B12 B24 B36
}                             // B13 B25 B37

function cb_map_foreach(key, val) {
  m.forEach(cb_map_2);        // B2  B14 B26
  var back = "back.";         // B12 B24 B36
}                             // B13 B25 B37

function testSetNested() {
  debugger;                   // B0
  s.forEach(cb_set_foreach);  // B1
}                             // B38

function testMapNested() {
  debugger;                   // B0
  m.forEach(cb_map_foreach);  // B1
}                             // B38
`;

(async function test() {
  await Protocol.Debugger.enable();

  let breaks = 0;
  let expectedBreaks = 0;
  let allMatched = true;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let loc = callFrames[0].location;
    let line = source.split('\n')[loc.lineNumber];
    InspectorTest.log(line.trim());
    if (!line.includes(`B${breaks}`)) {
      allMatched = false;
      InspectorTest.log(`FAILED: expected B${breaks} but got: ${line.trim()}`);
    }
    breaks++;
    if (breaks < expectedBreaks) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  InspectorTest.log('--- Testing Set.prototype.forEach ---');
  breaks = 0;
  expectedBreaks = 12;
  await Protocol.Runtime.evaluate({expression: 'testSet()'});
  InspectorTest.log(`breaks: ${breaks}`);

  InspectorTest.log('--- Testing Map.prototype.forEach ---');
  breaks = 0;
  expectedBreaks = 12;
  await Protocol.Runtime.evaluate({expression: 'testMap()'});
  InspectorTest.log(`breaks: ${breaks}`);

  InspectorTest.log('--- Testing nested Set.prototype.forEach ---');
  breaks = 0;
  expectedBreaks = 39;
  await Protocol.Runtime.evaluate({expression: 'testSetNested()'});
  InspectorTest.log(`breaks: ${breaks}`);

  InspectorTest.log('--- Testing nested Map.prototype.forEach ---');
  breaks = 0;
  expectedBreaks = 39;
  await Protocol.Runtime.evaluate({expression: 'testMapNested()'});
  InspectorTest.log(`breaks: ${breaks}`);

  InspectorTest.log(`allMatched: ${allMatched}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
