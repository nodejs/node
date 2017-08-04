'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const helper = require('./inspector-helper.js');

const ROOT_CONTEXT_NAME = 'Node.js Main Context';
const ROOT_CONTEXT_ORIGIN = '';
const VM_CONTEXT_NAME = 'New VM Context';
const VM_CONTEXT_ORIGIN = 'https://nodejs.org/api/vm.html';

let topContextId;
let vmContextId;

function setupExpectContextCreation(idx) {
  return (message) => {
    if (message.method === 'Runtime.executionContextCreated') {
      const { context } = message.params;
      assert.strictEqual(typeof context.id, 'number');
      if (idx === 0) {
        topContextId = context.id;
        assert.strictEqual(context.name, ROOT_CONTEXT_NAME);
        assert.strictEqual(context.origin, ROOT_CONTEXT_ORIGIN);
        assert.strictEqual(context.auxData.isDefault, true);
      } else {
        vmContextId = context.id;
        assert.strictEqual(context.name, VM_CONTEXT_NAME);
        assert.strictEqual(context.origin, VM_CONTEXT_ORIGIN);
        assert.strictEqual(context.auxData.isDefault, false);
      }
      return true;
    }
  };
}

function setupExpectBreakInContext(functionName) {
  return (message) => {
    if (message.method === 'Debugger.paused') {
      if (functionName) {
        const callFrame = message.params.callFrames[0];
        assert.strictEqual(callFrame.functionName, functionName);
      }
      return true;
    }
  };
}

function setupExpectContextDestroyed(idx) {
  return (message) => {
    if (message.method === 'Runtime.executionContextDestroyed') {
      const { executionContextId } = message.params;
      assert.strictEqual(executionContextId,
                         idx === 0 ? topContextId : vmContextId);
      return true;
    }
  };
}

function testBreakpointInContext(session) {
  console.log('[test]',
              'Verifying debugger stops on start (--inspect-brk option)');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
  ];
  const messages = [
    setupExpectContextCreation(0),
    setupExpectContextCreation(1),
    setupExpectBreakInContext('testfn'),
  ];
  session
    .sendInspectorCommands(commands)
    .expectMessages(messages);
}

function resume(session) {
  console.log('[test]', 'Resuming and waiting for pause');
  session
    .sendInspectorCommands({ 'method': 'Debugger.resume' })
    .expectMessages(setupExpectBreakInContext());
}

function testEvaluateInContext(session) {
  console.log('[test]', 'Evaluate in VM context');
  session.sendInspectorCommands([
    [
      { method: 'Runtime.evaluate',
        params: { expression: 'propInSandbox++',
                  contextId: vmContextId } },
      ({ result }) => {
        assert.strictEqual(result.type, 'number');
        assert.strictEqual(result.value, 42);
      }
    ],
    [
      { method: 'Runtime.evaluate',
        params: { expression: 'sandbox.propInSandbox' } },
      ({ result }) => {
        assert.strictEqual(result.type, 'number');
        assert.strictEqual(result.value, 43);
      }
    ]
  ]);
}

function testDetachContext(session) {
  console.log('[test]', 'Verify V8 acknowledges context detachment');
  session
    .sendInspectorCommands({ 'method': 'Debugger.resume' })
    .expectMessages([
      setupExpectContextDestroyed(1),
      setupExpectBreakInContext()
    ]);
}

function resumeAndTerminate(session) {
  session
    .sendInspectorCommands({ 'method': 'Debugger.resume'})
    .disconnect(true);
}

function runTests(harness) {
  harness
    .runFrontendSession([
      testBreakpointInContext,
      resume,
      testEvaluateInContext,
      testDetachContext,
      resumeAndTerminate
    ]).expectShutDown(0);
}

const script = `
'use strict';
const { attachContext, detachContext } = require('inspector');
const { createContext, runInContext } = require('vm');

const sandbox = {
  propInSandbox: 42
};
createContext(sandbox);

attachContext(sandbox, {
  name: ${JSON.stringify(VM_CONTEXT_NAME)},
  origin: ${JSON.stringify(VM_CONTEXT_ORIGIN)}
});
runInContext(\`
  function testfn() {
    debugger;
  }
  testfn();
\`, sandbox);
debugger;
detachContext(sandbox);
debugger;
`;

helper.startNodeForInspectorTest(runTests, '--inspect-brk', script);
