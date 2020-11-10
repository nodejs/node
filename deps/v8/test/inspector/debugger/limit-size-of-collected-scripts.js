// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that inspector does not retain old collected scripts.\n');

(async function main() {
  InspectorTest.log('Limit scripts cache size to 1MB');
  const maxScriptsCacheSize = 1e6;
  Protocol.Debugger.enable({maxScriptsCacheSize});
  const scriptIds = [];
  Protocol.Debugger.onScriptParsed(message => scriptIds.push(message.params.scriptId));

  InspectorTest.log('Generate 15 scripts 100KB each');
  await Protocol.Runtime.evaluate({
      expression: 'for (let i = 0; i < 15; ++i) eval(`"${new Array(5e4).fill("й").join("")}".length`);'});
  await Protocol.HeapProfiler.collectGarbage();
  const firstPhaseScripts = scriptIds.length;

  InspectorTest.log('Generate 15 more scripts 100KB each');
  await Protocol.Runtime.evaluate({
      expression: 'for (let i = 0; i < 15; ++i) eval(`"${new Array(5e4).fill("й").join("")}".length`);'});
  await Protocol.HeapProfiler.collectGarbage();

  InspectorTest.log('Check that earlier scripts are gone');
  InspectorTest.logMessage(`Total scripts size < 1KB: ${await sizeOfScripts(scriptIds.slice(0, firstPhaseScripts)) < 1e3}`);

  InspectorTest.log('Check that some of the later scripts are still there');
  InspectorTest.logMessage(`Total scripts size > 850KB: ${await sizeOfScripts(scriptIds) > 850e3}`);

  InspectorTest.completeTest();

  async function sizeOfScripts(scriptIds) {
    let size = 0;
    for (const scriptId of scriptIds) {
      const result = await Protocol.Debugger.getScriptSource({scriptId});
      size += result.result ? result.result.scriptSource.length * 2 : 0;
    }
    return size;
  }
})();
