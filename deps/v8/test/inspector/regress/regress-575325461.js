// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatters cannot return huge JSONML arrays.');

contextGroup.addScript(`
  function hugeSparseArray(tag, length) {
    const a = [tag];
    a[length - 1] = 0;
    return a;
  }

  globalThis.devtoolsFormatters = [{
    header(object) {
      switch (object.kind) {
        case 'huge-header':
          return hugeSparseArray('div', 0xFFFFFFFF);
        case 'nested-header': {
          const a = hugeSparseArray('div', 6e4);
          a[1] = hugeSparseArray('span', 6e4);
          return a;
        }
        default:
          return ['span', object.kind];
      }
    },
    hasBody(object) {
      return object.kind === 'huge-body';
    },
    body(object) {
      return hugeSparseArray('div', 0xFFFFFFFF);
    }
  }];
`);

Protocol.Runtime.onConsoleAPICalled(
    ({params: {type, args}}) => InspectorTest.log(`${type}: ${args[0].value}`));

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

  async function testHugeHeader() {
    await evaluateWithCustomPreview('huge-header');
  },

  async function testNestedHeader() {
    await evaluateWithCustomPreview('nested-header');
  },

  async function testHugeBody() {
    const {objectId, customPreview: {bodyGetterId}} =
        await evaluateWithCustomPreview('huge-body');
    const {result: {result}} = await Protocol.Runtime.callFunctionOn({
      objectId,
      functionDeclaration:
          'function(bodyGetter) { return bodyGetter.call(this); }',
      arguments: [{objectId: bodyGetterId}],
      returnByValue: true
    });
    InspectorTest.logMessage(result);
  },
]);
