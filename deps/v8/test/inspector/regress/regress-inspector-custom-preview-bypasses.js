// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatters cannot bypass length limits via object tags or nested escaping.');

contextGroup.addScript(`
  globalThis.devtoolsFormatters = [
    {
      header(object) {
        if (object.kind === 'direct_string') {
          const s = 'x'.repeat(4e6);
          return ['div', s, s, s];
        }
        if (object.kind === 'object_tags') {
          const s = 'x'.repeat(4e6);
          return ['div',
            ['object', {object: s}],
            ['object', {object: s}],
            ['object', {object: s}]
          ];
        }
        if (object.kind === 'nested_header') {
          if (object.depth > 0) {
            return ['div', ['object', {object: {kind: 'nested_header', depth: object.depth - 1, s: object.s}}]];
          }
          return ['div', object.s];
        }
        return null;
      },
      hasBody() { return false; },
    }
  ];
`);

async function evaluateWithCustomPreview(expression, bypassName) {
  const {result: {result}} =
      await Protocol.Runtime.evaluate({expression, generatePreview: true});
  if (result.customPreview) {
    const len = result.customPreview.header.length;
    InspectorTest.logMessage(`[${bypassName}] customPreview length: ${len}`);
    if (len > 10_000_000) {
      InspectorTest.logMessage(
          `FAILURE: ${bypassName} bypassed the 10M budget!`);
    }
  } else {
    InspectorTest.logMessage(`[${bypassName}] no custom preview`);
  }
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  },

  async function testDirectString() {
    await evaluateWithCustomPreview(
        `({kind: 'direct_string'})`, 'DirectString');
  },

  async function testObjectTags() {
    await evaluateWithCustomPreview(`({kind: 'object_tags'})`, 'ObjectTags');
  },

  async function testNestedHeader() {
    contextGroup.addScript(`
      var bad_string = '\\\\\\\\'.repeat(2e6); // 2 million backslashes
    `);
    await evaluateWithCustomPreview(
        `({kind: 'nested_header', depth: 4, s: bad_string})`, 'NestedHeader');
  },
]);
