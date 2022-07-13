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
  InspectorTest.completeTest();
})();
