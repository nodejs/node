// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that Debugger.evaluateOnCallFrame respects the TDZ of module ' +
    'bindings.');

const moduleName = 'evaluate-on-call-frame-module-tdz.mjs';

// The module imports from itself so that it also has an import binding that
// is still in its TDZ while the module body runs.
const moduleSource = `
import {exportedLet as importedLet} from '${moduleName}';

function host(param) {
  globalThis.readParam = () => param;
  debugger;
}
host(1);
debugger;

export let exportedLet = 2;
export const exportedConst = 3;
export class ExportedClass {}
debugger;
`;

async function evaluate(callFrameId, expression) {
  const response =
      await Protocol.Debugger.evaluateOnCallFrame({callFrameId, expression});
  if (response.error) {
    InspectorTest.log(`  ${expression} => error ${response.error.message}`);
    return;
  }
  const {result, exceptionDetails} = response.result;
  if (exceptionDetails) {
    const [message] = exceptionDetails.exception.description.split('\n');
    InspectorTest.log(`  ${expression} => throws ${message}`);
  } else {
    InspectorTest.log(`  ${expression} => ${result.value}`);
  }
}

async function evaluateAllOnPause(title, expressions) {
  const {params: {callFrames: [{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();
  InspectorTest.log(title);
  for (const expression of expressions) {
    await evaluate(callFrameId, expression);
  }
  await Protocol.Debugger.resume();
}

(async function test() {
  await Protocol.Debugger.enable();
  contextGroup.addModule(moduleSource, moduleName);

  await evaluateAllOnPause('Paused in a function, bindings in TDZ:', [
    // Reads.
    'exportedLet',
    'typeof exportedLet',
    'exportedConst',
    'ExportedClass',
    'importedLet',
    // Writes.
    'exportedLet = 4',
    'exportedConst = 4',
    'importedLet = 4',
    // The value of an uninitialized binding must not reach other variables.
    'param = exportedLet',
    'param = importedLet, param = 5',
    'param',
  ]);

  await evaluateAllOnPause('Paused at module top-level, bindings in TDZ:', [
    'exportedLet',
    'importedLet',
    'exportedLet = 4',
    'readParam()',
  ]);

  await evaluateAllOnPause('Paused at module top-level, initialized:', [
    'exportedLet',
    'importedLet',
    'exportedConst',
    'typeof ExportedClass',
    'exportedLet = 4',
    'importedLet',
    'exportedConst = 4',
    'importedLet = 5',
    'readParam()',
  ]);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
