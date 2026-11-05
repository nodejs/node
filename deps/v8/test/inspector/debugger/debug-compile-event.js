// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scriptParsed and scriptFailedToParse events for eval and ' +
    'JSON.parse.');

contextGroup.addScript(
    `
function compileSource(source) {
  eval(source);
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let after_compile_count = 0;
  let compile_error_count = 0;
  let source_count = 0;
  const parsedEvents = [];
  const failedEvents = [];

  Protocol.Debugger.onScriptParsed(params => {
    parsedEvents.push(params.params);
  });
  Protocol.Debugger.onScriptFailedToParse(params => {
    failedEvents.push(params.params);
  });

  async function runCompile(source, extraEvalEvents = 0) {
    parsedEvents.length = 0;
    failedEvents.length = 0;
    await Protocol.Runtime.evaluate(
        {expression: `compileSource(${JSON.stringify(source)})`});
    // Ignore the outer Runtime.evaluate wrapper script (first parsed event).
    const innerParsed = parsedEvents.slice(1);
    const innerFailed = failedEvents;
    if (innerFailed.length === 0) {
      source_count += 1 + extraEvalEvents;
    }
    for (const ev of innerParsed) {
      after_compile_count++;
      const {result: {scriptSource}} =
          await Protocol.Debugger.getScriptSource({scriptId: ev.scriptId});
      const matchesSource = source.indexOf('eval') === 0 ?
          source.indexOf(scriptSource) >= 0 :
          source === scriptSource;
      InspectorTest.log(`scriptParsed: matches=${matchesSource}, url='${
          ev.url}', source=${JSON.stringify(scriptSource)}`);
    }
    for (const ev of innerFailed) {
      compile_error_count++;
      const {result: {scriptSource}} =
          await Protocol.Debugger.getScriptSource({scriptId: ev.scriptId});
      const matchesSource = source === scriptSource;
      InspectorTest.log(`scriptFailedToParse: matches=${
          matchesSource}, source=${JSON.stringify(scriptSource)}`);
    }
  }

  await runCompile('a=1');
  await runCompile('(function(){})');
  await runCompile('eval("a=2")', 1);
  await runCompile('eval("eval(\'(function(){return a;})\')")', 2);
  await runCompile('JSON.parse(\'{"a":1,"b":2}\')');
  await runCompile('x=1; //# sourceURL=myscript.js');
  await runCompile('}');

  InspectorTest.log(`compile_error_count: ${compile_error_count}`);
  InspectorTest.log(`source_count === after_compile_count: ${
      source_count === after_compile_count} (${after_compile_count})`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
