// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that custom formatter errors with too long messages are reported.');

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
  Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
    InspectorTest.log(`${type}: ${args.map(({value}) => value).join(' ')}`);
  });

  // Throw a string such that the message for the uncaught exception (i.e.
  // "Uncaught " followed by the thrown string) has the maximum string length.
  await Protocol.Runtime.evaluate({
    expression: `
      const error = 'a'.repeat(%StringMaxLength() - 'Uncaught '.length);
      globalThis.devtoolsFormatters = [{
        header(object) {
          throw error;
        }
      }];
    `
  });
  await Protocol.Runtime.evaluate({expression: '({})', generatePreview: true});

  InspectorTest.completeTest();
})();
