// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
  InspectorTest.start('Tests that destroying context during custom preview wrapping does not cause UAF.');

(async function test() {
  await Protocol.Runtime.enable();
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.devtoolsFormatters = [{
        header: function(obj) {
          return ['div', {}, 'header'];
        },
        hasBody: function(obj) {
          inspector.fireContextDestroyed();
          return true;
        },
        body: function(obj) {
          return ['div', {}, 'body'];
        }
      }];
    `
  });

  await Protocol.Runtime.evaluate({
    expression: '({a: 1})',
    generatePreview: true
  });

  InspectorTest.completeTest();
})();
