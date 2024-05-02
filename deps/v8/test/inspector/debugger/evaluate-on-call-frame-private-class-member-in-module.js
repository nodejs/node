// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { contextGroup, Protocol } = InspectorTest.start(
  'Evaluate private class member out of class scope in Debugger.evaluateOnCallFrame() in module'
);

Protocol.Debugger.enable();
const source = `
class Klass {
  #field = 1;
}
const obj = new Klass;
debugger;
`;

InspectorTest.log(source);
contextGroup.addModule(source, 'module');

InspectorTest.runAsyncTestSuite([async function evaluatePrivateMembers() {
  const { params: { callFrames } } = await Protocol.Debugger.oncePaused();
  const frame = callFrames[0];
  const expression = 'obj.#field';
  InspectorTest.log(`Debugger.evaluateOnCallFrame: \`${expression}\``);
  const { result: { result } } =
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: frame.callFrameId,
      expression
    });
  InspectorTest.logMessage(result);
}]);
