// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatter body does not crash on huge strings.');

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.map(({value}) => value).join(' ')}`);
  });

  await Protocol.Runtime.evaluate({
    expression: `
      const s = 'a'.repeat(%StringMaxLength());
      globalThis.devtoolsFormatters = [{
        header(object) {
          return ['span', 'x'];
        },
        hasBody(object) {
          return true;
        },
        body(object) {
          return ['div', s];
        }
      }];
    `
  });

  const result = await Protocol.Runtime.evaluate(
      {expression: '({})', generatePreview: true});

  const customPreview = result.result.result.customPreview;
  if (customPreview && customPreview.bodyGetterId) {
    const objectId = result.result.result.objectId;
    const body = await Protocol.Runtime.callFunctionOn({
      objectId,
      functionDeclaration:
          'function(bodyGetter) { return bodyGetter.call(this); }',
      arguments: [{objectId: customPreview.bodyGetterId}],
      returnByValue: true
    });
    InspectorTest.logMessage(body.result.result);
  }

  InspectorTest.completeTest();
})();
