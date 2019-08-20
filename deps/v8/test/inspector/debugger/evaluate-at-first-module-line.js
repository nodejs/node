// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
  InspectorTest.start('Evaluate at first line of module should not crash');

const utilsModule = `export function identity(value) {
  return value;
}`;

const mainModule = `import {identity} from 'utils';
console.log(identity(0));`;

(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 1,
    url: 'main'
  });

  contextGroup.addModule(utilsModule, 'utils');
  contextGroup.addModule(mainModule, 'main');
  const { params: { callFrames } } = await Protocol.Debugger.oncePaused();
  const result = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: callFrames[0].callFrameId,
    expression: 'identity(0)'
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})()
