// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --max-old-space-size=64

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatters budget correctly charges for ' +
    'undefined/function properties.');

contextGroup.addScript(`
  function makeObjectWithUndefinedProperties(count) {
    const leaf = {};
    for (let i = 0; i < count; i++) {
      leaf[i] = undefined;
    }
    const a = ['div'];
    for (let i = 0; i < 500; i++) {
      a.push(leaf);
    }
    return a;
  }

  globalThis.devtoolsFormatters = [{
    header(object) {
      if (object.kind === 'shared-60k') {
        return makeObjectWithUndefinedProperties(60000);
      }
      if (object.kind === 'DAG') {
        return makeObjectWithUndefinedProperties(1000000);
      }
      return null;
    },
    hasBody(object) { return false; },
    body(object) { return null; }
  }];
`);

async function evaluateWithCustomPreview(kind) {
  InspectorTest.log('Evaluating ' + kind);
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

  async function testShared60k() {
    await evaluateWithCustomPreview('shared-60k');
  },

  async function testDAG() {
    await evaluateWithCustomPreview('DAG');
  },
]);
