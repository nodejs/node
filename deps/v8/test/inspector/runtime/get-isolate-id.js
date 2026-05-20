// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start('Checks that Runtime.getIsolateId works');

(async function test() {
  const id = await Protocol.Runtime.getIsolateId();
  console.assert(typeof id === 'string');
  console.assert(id.length > 0);
  InspectorTest.completeTest();
})();
