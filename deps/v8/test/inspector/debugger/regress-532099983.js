// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-532099983.');

const source = `
let victims = [];
for (let i = 0; i < 2000; i++) {
  victims.push(eval("(function victim_" + i + "() {})"));
}

function checkPolluted() {
  let polluted = 0;
  for (let i = 0; i < 2000; i++) {
    if (victims[i].prototype && victims[i].prototype.polluted) {
      polluted++;
    }
  }
  return polluted;
}
`;

contextGroup.addScript(source);

(async function test() {
  await Protocol.Runtime.enable();

  const code = `
    (function() {
      let keep = [];
      for (let i = 0; i < 50000; i++) {
        keep.push({a: i});
        keep.push({a: i, b: i});
        keep.push([i, i]);
      }
      keep = null;
      for (let i = 0; i < 100000; i++) {
        let dummy = {c: i};
      }
      for (let j = 0; j < 2000; j++) {
        try {
          let proto = victims[j].prototype;
          proto.polluted = true;
        } catch (e) {}
      }
    })()
  `;

  await Protocol.Runtime.evaluate({
    expression: code,
    throwOnSideEffect: true,
  });

  const {result: {result: {value: polluted}}} =
      await Protocol.Runtime.evaluate({expression: 'checkPolluted()'});
  InspectorTest.log('polluted: ' + polluted);

  InspectorTest.completeTest();
})().catch(e => {
  InspectorTest.log('Uncaught: ' + e);
  InspectorTest.completeTest();
});
