// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('crbug.com/736302');

InspectorTest.runAsyncTestSuite([
  async function testThrowException() {
    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: `
    console.count({
      get [Symbol.toStringTag]() {
        throw new Error();
      }
    });`});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  },

  async function testCustomName() {
    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: `
    console.count({
      get [Symbol.toStringTag]() {
        return 'MyObject';
      }
    });`});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  },

  async function testObject() {
    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({expression: `
    console.count({});`});
    InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());
    await Protocol.Runtime.disable();
  }

]);
