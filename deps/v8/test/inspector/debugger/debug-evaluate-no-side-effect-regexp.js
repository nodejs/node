// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-regexp.');

contextGroup.addScript(`
function getRegExpState() {
  return JSON.stringify([
    RegExp.$1,
    RegExp.$2,
    RegExp.$3,
    RegExp.$4,
    RegExp.$5,
    RegExp.$6,
    RegExp.$7,
    RegExp.$8,
    RegExp.$9,
    RegExp.lastMatch,
    RegExp.lastParen,
    RegExp.leftContext,
    RegExp.rightContext,
    RegExp.input,
  ]);
}
`);

(async function test() {
  await Protocol.Runtime.evaluate({
    expression:
        `/(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)/.exec(">>>abcdefghij<<<")`,
  });
  const before =
      await Protocol.Runtime.evaluate({expression: 'getRegExpState()'});
  InspectorTest.log('before: ' + before.result.result.value);

  await Protocol.Runtime.evaluate({
    expression:
        `/(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)(\\w)/.exec(">>>hklmnoprst<<<")`,
    throwOnSideEffect: true,
  });

  const after =
      await Protocol.Runtime.evaluate({expression: 'getRegExpState()'});
  InspectorTest.log('after: ' + after.result.result.value);
  InspectorTest.completeTest();
})();
