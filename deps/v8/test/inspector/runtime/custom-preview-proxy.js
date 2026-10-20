// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } =
  InspectorTest.start('Checks that proxies in JSONML are rejected by custom formatters');

(async function test() {
  Protocol.Runtime.enable();
  Protocol.Runtime.setCustomObjectFormatterEnabled({ enabled: true });
  Protocol.Runtime.onConsoleAPICalled(m => InspectorTest.logMessage(m));

  InspectorTest.log('Test proxy child in header JSONML..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, 'Header with proxy child', new Proxy([], {})],
        hasBody: () => false
      }];
    `
  });
  let result = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(result);

  InspectorTest.log('Test proxy attributes in header JSONML..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', new Proxy({}, {}), 'Header with proxy attributes'],
        hasBody: () => false
      }];
    `
  });
  result = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(result);

  InspectorTest.log('Test proxy array child masquerading as tag..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, new Proxy(['object', {object: {}}], {})],
        hasBody: () => false
      }];
    `
  });
  result = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(result);

  InspectorTest.log('Test proxy attributes in object tag..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, ['object', new Proxy({object: {}}, {})]],
        hasBody: () => false
      }];
    `
  });
  result = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  dumpCustomPreview(result);

  InspectorTest.log('Test proxy child in body JSONML..');
  await Protocol.Runtime.evaluate({
    expression: `
      this.devtoolsFormatters = [{
        header: () => ['span', {}, 'Header ok'],
        hasBody: () => true,
        body: () => ['span', {}, 'Body with proxy child', new Proxy([], {})]
      }];
    `
  });
  result = await Protocol.Runtime.evaluate({ expression: '({})', generatePreview: true });
  await dumpCustomPreviewWithBody(result);

  InspectorTest.log('Test formatting a proxy object itself (allowed)..');
  await Protocol.Runtime.evaluate({
    expression: `
      var targetProxy = new Proxy({name: 'proxyObject'}, {});
      this.devtoolsFormatters = [{
        header: (x) => x === targetProxy ? ['span', {}, 'Proxy formatted: ', x.name] : null,
        hasBody: () => false
      }];
    `
  });
  result = await Protocol.Runtime.evaluate({ expression: 'targetProxy', generatePreview: true });
  dumpCustomPreview(result);

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
