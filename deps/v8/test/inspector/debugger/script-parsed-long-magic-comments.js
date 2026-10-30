// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start(
    'Tests long magic comment values in Debugger.scriptParsed');

const value = 'data:application/json;base64,' + 'A'.repeat(100000);
const source = `const foo = 42;
//# sourceURL=${value}
//# sourceMappingURL=${value}
//# debugId=${value}
`;

function describe(actual) {
  return actual === value ? 'full value' :
                            `unexpected value of length ${actual.length}`;
}

contextGroup.addScript(source, 0, 0, 'script.js');
contextGroup.addModule(source, 'module.js');

(async () => {
  Protocol.Debugger.onScriptParsed(
      ({params: {url, hasSourceURL, sourceMapURL, buildId}}) => {
        InspectorTest.log('Debugger.scriptParsed with:');
        InspectorTest.log('url: ' + describe(url));
        InspectorTest.log('hasSourceURL: ' + hasSourceURL);
        InspectorTest.log('sourceMapURL: ' + describe(sourceMapURL));
        InspectorTest.log('buildId: ' + describe(buildId));
        InspectorTest.log();
      });

  await Protocol.Debugger.enable();
  InspectorTest.completeTest();
})();
