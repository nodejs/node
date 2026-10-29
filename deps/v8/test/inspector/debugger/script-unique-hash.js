// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Checks hash in Debugger.scriptParsed event');

(async function main() {
  Protocol.Debugger.enable();

  const tests = [{firstScript: '1', secondScript: '2'}];
  for (let length = 1; length <= 10; ++length) {
    for (let differentChar = 0; differentChar < length; ++differentChar) {
      const firstScript = ' '.repeat(differentChar) + '1' +
          ' '.repeat(length - differentChar - 1) + ';';
      const secondScript = ' '.repeat(differentChar) + '2' +
          ' '.repeat(length - differentChar - 1) + ';';
      tests.push({firstScript, secondScript});
    }
  }

  for (const {firstScript, secondScript} of tests) {
    InspectorTest.log(firstScript);
    const firstScriptParsed = Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({expression: firstScript});
    const hash1 = (await firstScriptParsed).params.hash;

    InspectorTest.log(secondScript);
    const secondScriptParsed = Protocol.Debugger.onceScriptParsed();
    Protocol.Runtime.evaluate({expression: secondScript});
    const hash2 = (await secondScriptParsed).params.hash;

    InspectorTest.log(hash1 === hash2 ? 'Error: the same hash!' : 'PASS');
  }
  InspectorTest.completeTest();
})();
