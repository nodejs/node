// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we report correct endLine, endColumn and source for scripts.');

var sources = [
'',
' ',
'  ',
`
`,
`
 `,
`
   `,
`

`,
`

 `,
`

  `];

(async function test() {
  Protocol.Debugger.enable();
  for (let source of sources) {
    contextGroup.addScript(source);
    var message = await Protocol.Debugger.onceScriptParsed();
    var inspectorSource = (await Protocol.Debugger.getScriptSource({ scriptId: message.params.scriptId })).result.scriptSource;
    var lines = source.split('\n');
    var returned = { endLine: message.params.endLine, endColumn: message.params.endColumn };
    var compiled = { endLine: lines.length - 1, endColumn: lines[lines.length - 1].length };
    InspectorTest.logObject({ returned, compiled });
    if (returned.endLine != compiled.endLine) {
      InspectorTest.log('error: incorrect endLine');
    }
    if (returned.endColumn != compiled.endColumn) {
      InspectorTest.log('error: incorrect endColumn');
    }
    if (source !== inspectorSource) {
      InspectorTest.log('error: incorrect source');
    }
  }
  InspectorTest.completeTest();
})();
