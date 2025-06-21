// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks console.memory');

InspectorTest.runAsyncTestSuite([
  async function testWithoutMemory() {
    InspectorTest.logMessage(
      await Protocol.Runtime.evaluate({expression: 'console.memory'}));
  },

  async function testSetterInStrictMode() {
    // crbug.com/468611
    InspectorTest.logMessage(
      await Protocol.Runtime.evaluate({
        expression: '"use strict"\nconsole.memory = {};undefined' }));
  },

  async function testWithMemory() {
    utils.setMemoryInfoForTest(239);
    InspectorTest.logMessage(
      await Protocol.Runtime.evaluate({expression: 'console.memory'}));
  },

  async function testSetterDoesntOverride() {
    utils.setMemoryInfoForTest(42);
    await Protocol.Runtime.evaluate({expression: 'console.memory = 0'});
    InspectorTest.logMessage(
      await Protocol.Runtime.evaluate({expression: 'console.memory'}));
  }
]);
