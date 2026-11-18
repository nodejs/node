// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start('Checks that Runtime.getHeapUsage works');

(async function test() {
  const heapUsage = await Protocol.Runtime.getHeapUsage();
  const usedSize = heapUsage.result["usedSize"];
  const totalSize = heapUsage.result["totalSize"];
  console.assert(usedSize > 0);
  console.assert(totalSize > 0);
  console.assert(totalSize >= usedSize);

  InspectorTest.log('getHeapUsage is not allowed for untrusted clients');
  const untrustedSession = contextGroup.connect(/* isFullyTrusted */ false);
  InspectorTest.logMessage(
      await untrustedSession.Protocol.Runtime.getHeapUsage());
  untrustedSession.disconnect();

  InspectorTest.completeTest();
})();
