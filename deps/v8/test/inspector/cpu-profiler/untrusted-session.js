// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.start(
    'Tests that Profiler domain is not available in untrusted session.');

(async () => {
  const contextGroup = new InspectorTest.ContextGroup();
  const session = contextGroup.connect(/* isFullyTrusted */ false);
  const {Protocol} = session;

  await Protocol.Runtime.evaluate({
    expression: 'console.profile("test"); console.profileEnd("test");'
  });
  InspectorTest.log('console.profile in untrusted session did not crash.');
  await Protocol.Profiler.enable();

  InspectorTest.completeTest();
})();
