// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
  'Test that the test runner prints if a non-existent method is called');

(async function test() {
  await Protocol.Runtime.method_does_not_exist();

  // This will only be called if the test fails, because the test runner should
  // call completeTest() if the method does not exist.
  InspectorTest.completeTest();
})();
