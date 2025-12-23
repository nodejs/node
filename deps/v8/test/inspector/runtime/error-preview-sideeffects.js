// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that error preview does not run user code');

contextGroup.addScript(`
function testLogError() {
  let stackCalled = false;

  {
    const error = new Error();
    Object.defineProperty(error, 'stack', {
      get: () => {
        stackCalled = true;
        return Reflect.get(error, 'stack');
      },
    });

    console.log(error);
  }

  let stackCalledOnBase = false;
  {
    const base = new Error();
    Object.defineProperty(base, 'stack', {
      get: () => {
        stackCalledOnBase = true;
        return 'value';
      },
    });

    const error = Object.create(base);
    console.log(error);
  }

  let stackCalledOnProxy = false;
  {
    const error = new Proxy(new Error(), {
      get: (target, prop) => {
        if (prop !== 'stack')
          return Reflect.get(target, prop);
        stackCalledOnProxy = true;
        return 'value';
      },
    });

    console.log(error);
  }

  console.clear();

  return {stackCalled, stackCalledOnBase, stackCalledOnProxy};
}

//# sourceURL=test.js
`);

InspectorTest.runAsyncTestSuite([
  async function ErrorStack() {
    await Protocol.Runtime.enable();
    const result = await Protocol.Runtime.evaluate({expression: 'testLogError()', returnByValue: true});
    InspectorTest.logObject(result.result.result.value);
    await Protocol.Runtime.disable();
  }
]);
