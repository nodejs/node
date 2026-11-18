// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test setting "Never Pause Here" breakpoint in Wasm.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

const source = `
const builder = new WasmModuleBuilder();
const body_a = [
  kExprUnreachable,  // unreachable
];
const fun_a = builder.addFunction('a', kSig_v_v).addBody(body_a).exportFunc();
const instance = builder.instantiate();
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  let wasmScriptId = null;
  Protocol.Debugger.onScriptParsed(message => {
    if (message.params.url.startsWith('wasm://')) {
      wasmScriptId = message.params.scriptId;
    }
  });

  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  const {result: {result: funABodyOffset}} =
      await Protocol.Runtime.evaluate({expression: 'fun_a.body_offset;'});

  const expected_breaks = [
    `$a:1:${funABodyOffset.value}`,
  ];

  let error = null;
  Protocol.Debugger.onPaused(async message => {
    try {
      if (message.params.reason !== 'exception') {
        error = new Error('Unexpected pause reason: ' + message.params.reason);
        await Protocol.Debugger.resume();
        return;
      }
      const topFrame = message.params.callFrames[0];
      const pos = [
        topFrame.functionName, topFrame.location.lineNumber + 1,
        topFrame.location.columnNumber
      ].join(':');
      InspectorTest.log(`Break at ${pos}:`);
      if (expected_breaks.length === 0) {
        error = new Error('Unexpected extra break');
      } else {
        const expected_pos = expected_breaks.shift();
        if (expected_pos !== pos) {
          error = new Error(`Expected break at ${expected_pos}, got ${pos}`);
        }
      }
    } catch (e) {
      if (!error) error = e;
    }
    await Protocol.Debugger.resume();
  });

  async function testWrapper() {
    InspectorTest.log('Running a().');
    const {result} = await Protocol.Runtime.evaluate({
      expression: `(() => {
        try {
          instance.exports.a();
          return 'Exited normally???';
        } catch (e) {
          return 'Exception thrown';
        }
      })()`,
    });
    InspectorTest.log(result.result.value);
    InspectorTest.log('Returned from wasm.');
  }

  await testWrapper();
  if (expected_breaks.length !== 0) {
    InspectorTest.log('ERROR: not all breaks were hit');
  }

  await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId: wasmScriptId,
      lineNumber: 0,
      columnNumber: funABodyOffset.value,
    },
    condition: 'false',
  });

  await testWrapper();

  if (error) {
    InspectorTest.log('FAIL: ' + error.message);
  }

  InspectorTest.completeTest();
})();
