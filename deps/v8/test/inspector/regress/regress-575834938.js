// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that nested custom formatters do not bypass JSONML budget and ' +
    'cause OOM.');

contextGroup.addScript(`
  const largeString = 'a'.repeat(1024 * 1024 * 10); // 10MB string

  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object.kind === 'outer') {
        const result = ['div'];
        // Generate enough children so that the total string size exceeds memory
        // if budgets are not shared. Each child will serialize the large string.
        for (let i = 0; i < 200; i++) {
          result.push(['object', { object: { kind: 'child' } }]);
        }
        return result;
      }
      if (object.kind === 'child') {
        return ['span', largeString];
      }
      return null;
    },
    hasBody() { return false; },
  }];
`);

async function evaluateWithCustomPreview(kind) {
  const {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: `({kind: '${kind}'})`, generatePreview: true});
  InspectorTest.logMessage(result.customPreview ?? 'no custom preview');
  return result;
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  },

  async function testOOM() {
    await evaluateWithCustomPreview('outer');
  },
]);
