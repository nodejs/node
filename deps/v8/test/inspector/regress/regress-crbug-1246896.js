// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-experimental-remove-internal-scopes-property

const {Protocol} = InspectorTest.start('Don\'t crash when getting the properties of a native function');

(async () => {
  const { result: { result: {objectId } } } = await Protocol.Runtime.evaluate({
    expression: '"".slice',
    objectGroup: 'console',
    includeCommandLineAPI: true,
  });

  const { result } = await Protocol.Runtime.getProperties({
    objectId,
    ownProperties: true,
    accessorPropertiesOnly: false,
    generatePreview: false,
  });

  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
