// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { Protocol } = InspectorTest.start('Don\'t crash when building a preview with a revoked proxy prototype');

(async () => {
  const { result: { result } } = await Protocol.Runtime.evaluate({
    expression: `
      const {proxy, revoke} = Proxy.revocable({}, {});
      revoke();
      Object.setPrototypeOf([], proxy);`,
    generatePreview: true,
  });
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
