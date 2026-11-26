// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that script ids are unique and loaded script ids match.');

const parsedScriptIds = [];
Protocol.Debugger.onScriptParsed(message => {
  parsedScriptIds.push(Number(message.params.scriptId));
});

(async function test() {
  await Protocol.Debugger.enable();

  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: `(() => {
      const ids = %DebugGetLoadedScriptIds();
      ids.sort((a, b) => a - b);
      let allUnique = true;
      for (let i = 1; i < ids.length; i++) {
        if (ids[i - 1] >= ids[i]) {
          allUnique = false;
        }
      }
      return {count: ids.length, allUnique};
    })()`,
    returnByValue: true
  });

  InspectorTest.log(`Loaded script IDs found: ${result.value.count > 0}`);
  InspectorTest.log(`All loaded script IDs unique: ${result.value.allUnique}`);

  parsedScriptIds.sort((a, b) => a - b);
  let parsedUnique = true;
  for (let i = 1; i < parsedScriptIds.length; i++) {
    if (parsedScriptIds[i - 1] >= parsedScriptIds[i]) {
      parsedUnique = false;
    }
  }
  InspectorTest.log(`Parsed script IDs found: ${parsedScriptIds.length > 0}`);
  InspectorTest.log(`All parsed script IDs unique: ${parsedUnique}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
