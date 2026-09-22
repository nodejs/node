// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Regression test for b/553931056.

const { session, contextGroup, Protocol } =
  InspectorTest.start('Tests that custom preview formatters prevent TOCTOU and sanitization bypasses');

(async function test() {
  Protocol.Runtime.enable();
  Protocol.Runtime.setCustomObjectFormatterEnabled({ enabled: true });
  Protocol.Runtime.onConsoleAPICalled(m => InspectorTest.logMessage(m));

  InspectorTest.log('1. Test microtask TOCTOU attack on body() and verify no objectId leak to page JS..');
  await Protocol.Runtime.evaluate({
    expression: `
      var microtaskObservedObjectId = 'not-run';
      this.devtoolsFormatters = [{
        header: () => ['span', {}, 'Header ok'],
        hasBody: () => true,
        body: () => {
          const trustedSlot = ['object', { object: { real: 1 } }];
          const victimSlot = ['span', {}, 'benign'];
          const tree = ['div', {}, trustedSlot, victimSlot];
          Promise.resolve().then(() => {
            microtaskObservedObjectId = trustedSlot[1].objectId;
            victimSlot.length = 0;
            victimSlot.push('object', {
              type: 'object',
              objectId: 'forged-id',
              description: 'forged-remote-object'
            });
          });
          return tree;
        }
      }];
    `
  });
  let evalRes = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  await dumpCustomPreviewWithBody(evalRes);
  let leakRes = await Protocol.Runtime.evaluate({ expression: 'String(microtaskObservedObjectId)' });
  InspectorTest.log('microtaskObservedObjectId in page JS: ' + leakRes.result.result.value);

  InspectorTest.log('2. Test toJSON() bypass attempt in header() and body()..');
  await Protocol.Runtime.evaluate({
    expression: `
      var toJSONCalled = false;
      const sneakyChild = ['span', {}, 'initial'];
      sneakyChild.toJSON = () => {
        toJSONCalled = true;
        return ['object', { type: 'object', objectId: 'forged-via-toJSON' }];
      };
      this.devtoolsFormatters = [{
        header: () => ['span', {}, sneakyChild],
        hasBody: () => true,
        body: () => ['div', {}, sneakyChild]
      }];
    `
  });
  evalRes = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  await dumpCustomPreviewWithBody(evalRes);
  let toJSONRes = await Protocol.Runtime.evaluate({ expression: 'toJSONCalled' });
  InspectorTest.log('toJSONCalled: ' + toJSONRes.result.result.value);

  InspectorTest.log('3. Test getter mutating earlier validated element during traversal..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => {
          const firstChild = ['span', {}, 'safe-child'];
          const tree = ['div', {}, firstChild];
          Object.defineProperty(tree, '3', {
            enumerable: true,
            get() {
              firstChild[0] = 'object';
              firstChild[1] = { type: 'object', objectId: 'forged-via-getter' };
              firstChild.length = 2;
              return 'trigger';
            }
          });
          return tree;
        },
        hasBody: () => false
      }];
    `
  });
  evalRes = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(evalRes);

  InspectorTest.log('4. Test new String("object") unboxing bypass attempt..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, [new String('object'), { type: 'object', objectId: 'forged-string-obj' }]],
        hasBody: () => false
      }];
    `
  });
  evalRes = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(evalRes);

  InspectorTest.log('5. Test ["object", forged, "extra"] (length != 2) bypass attempt..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, ['object', { type: 'object', objectId: 'forged-len-3' }, 'extra']],
        hasBody: () => false
      }];
    `
  });
  evalRes = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(evalRes);

  InspectorTest.completeTest();
})();

function dumpCustomPreview(result) {
  const remoteObject = result.result.result;
  if (remoteObject.customPreview) {
    InspectorTest.log('customPreview: ' + remoteObject.customPreview.header);
  } else {
    InspectorTest.log('No customPreview generated');
  }
}

async function dumpCustomPreviewWithBody(result) {
  const remoteObject = result.result.result;
  if (remoteObject.customPreview) {
    InspectorTest.log('customPreview: ' + remoteObject.customPreview.header);
    if (remoteObject.customPreview.bodyGetterId) {
      const body = await Protocol.Runtime.callFunctionOn({
        objectId: remoteObject.objectId,
        functionDeclaration: 'function(bodyGetter) { return bodyGetter.call(this); }',
        arguments: [{ objectId: remoteObject.customPreview.bodyGetterId }],
        returnByValue: true
      });
      InspectorTest.logMessage(body);
    }
  } else {
    InspectorTest.log('No customPreview generated');
  }
}
