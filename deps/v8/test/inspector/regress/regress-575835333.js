// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatters cannot return strings that cause OOM.');

contextGroup.addScript(`
  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object.kind === 'oom') {
        const s = 'x'.repeat(2**28 - 24);
        return ['div', s, s, s, s, s, s, s, s];
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
    await evaluateWithCustomPreview('oom');
  },
]);
