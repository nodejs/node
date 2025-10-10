// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start('Tests buildId for JavaScript scripts/modules');

contextGroup.addScript(`
const foo = 42;
//# debugId=85314830-023f-4cf1-a267-535f4e37bb17
`, 0, 0, 'script1.js');

contextGroup.addModule(`
const foo = 42;
//# debugId=cdc0c7f3-423a-4250-9f15-baac67a52cce
`, 'module1.js');

contextGroup.addScript(`
const foo = 42;
//@ debugId=d348ff8b-da3a-4c99-8dff-807b45723ed8
`, 0, 0, 'wrong-magic-comment.js');

(async () => {
  Protocol.Debugger.onScriptParsed(({params: { buildId, url }}) => {
    InspectorTest.log('Debugger.scriptParsed with:')
    InspectorTest.log('url: ' + url);
    InspectorTest.log('buildId: ' + buildId);
    InspectorTest.log();
  });

  Protocol.Debugger.onScriptFailedToParse(({params: { buildId, url }}) => {
    InspectorTest.log('Debugger.scriptFailedToParse with:')
    InspectorTest.log('url: ' + url);
    InspectorTest.log('buildId: ' + buildId);
    InspectorTest.log();
  });

  await Protocol.Debugger.enable();

  contextGroup.addScript(`
    for (
    //# debugId=715dd671-19f9-42dd-af76-93664d8a1bef
  `, 0, 0, 'failed-to-parse.js');

  InspectorTest.completeTest();
})();
