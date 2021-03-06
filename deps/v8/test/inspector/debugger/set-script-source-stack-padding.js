// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Change return string constant at breakpoint');

contextGroup.addScript(
`SlimFunction = eval(
  '(function f() {\\n ' +
  '  return \\'Cat\\';\\n' +
  '})\\n' +
  '//# sourceURL=eval.js\\n');`);

(async function test() {
  session.setupScriptMap();
  let scriptPromise = new Promise(resolve => {
    Protocol.Debugger.onScriptParsed(({params}) => {
      if (params.url === 'eval.js') {
        resolve(params);
        Protocol.Debugger.onScriptParsed(null);
      }
    });
  });
  Protocol.Debugger.enable();
  const script = await scriptPromise;

  InspectorTest.log('Set breakpoint inside f() and call function..');
  const {result:{actualLocation}} = await Protocol.Debugger.setBreakpoint({
    location: { scriptId: script.scriptId, lineNumber: 1, columnNumber: 0}});
  const evalPromise = Protocol.Runtime.evaluate({expression: 'SlimFunction()'});
  const {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused at:');
  await session.logSourceLocation(callFrames[0].location);

  InspectorTest.log('Change Cat to Capybara..');
  const {result:{scriptSource}} = await Protocol.Debugger.getScriptSource({
    scriptId: script.scriptId
  });
  const {result:{callFrames:[topFrame],stackChanged}} =
      await Protocol.Debugger.setScriptSource({
        scriptId: script.scriptId,
        scriptSource: scriptSource.replace(`'Cat'`, `'Capybara'`)
      });

  InspectorTest.log('Paused at:');
  await session.logSourceLocation(topFrame.location, true);

  InspectorTest.log('Resume and check return value..');
  Protocol.Debugger.resume();
  InspectorTest.log(
      `SlimFunction() = ${(await evalPromise).result.result.value}`);
  InspectorTest.completeTest();
})();
