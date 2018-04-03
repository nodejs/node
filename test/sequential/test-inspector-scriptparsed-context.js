// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.crashOnUnhandledRejection();
const { NodeInstance } = require('../common/inspector-helper.js');
const assert = require('assert');

const script = `
  'use strict';
  const assert = require('assert');
  const vm = require('vm');
  const { kParsingContext } = process.binding('contextify');
  global.outer = true;
  global.inner = false;
  const context = vm.createContext({
    outer: false,
    inner: true
  });
  debugger;

  const scriptMain = new vm.Script("outer");
  debugger;

  const scriptContext = new vm.Script("inner", {
    [kParsingContext]: context
  });
  debugger;

  assert.strictEqual(scriptMain.runInThisContext(), true);
  assert.strictEqual(scriptMain.runInContext(context), false);
  assert.strictEqual(scriptContext.runInThisContext(), false);
  assert.strictEqual(scriptContext.runInContext(context), true);
  debugger;

  vm.runInContext('inner', context);
  debugger;

  vm.runInNewContext('Array', {});
  debugger;

  vm.runInNewContext('debugger', {});
`;

async function getContext(session) {
  const created =
      await session.waitForNotification('Runtime.executionContextCreated');
  return created.params.context;
}

async function checkScriptContext(session, context) {
  const scriptParsed =
      await session.waitForNotification('Debugger.scriptParsed');
  assert.strictEqual(scriptParsed.params.executionContextId, context.id);
}

async function runTests() {
  const instance = new NodeInstance(['--inspect-brk=0', '--expose-internals'],
                                    script);
  const session = await instance.connectInspectorSession();
  await session.send([
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);
  await session.waitForBreakOnLine(0, '[eval]');

  await session.send({ 'method': 'Runtime.enable' });
  const topContext = await getContext(session);
  await session.send({ 'method': 'Debugger.resume' });
  const childContext = await getContext(session);
  await session.waitForBreakOnLine(13, '[eval]');

  console.error('[test]', 'Script associated with current context by default');
  await session.send({ 'method': 'Debugger.resume' });
  await checkScriptContext(session, topContext);
  await session.waitForBreakOnLine(16, '[eval]');

  console.error('[test]', 'Script associated with selected context');
  await session.send({ 'method': 'Debugger.resume' });
  await checkScriptContext(session, childContext);
  await session.waitForBreakOnLine(21, '[eval]');

  console.error('[test]', 'Script is unbound');
  await session.send({ 'method': 'Debugger.resume' });
  await session.waitForBreakOnLine(27, '[eval]');

  console.error('[test]', 'vm.runInContext associates script with context');
  await session.send({ 'method': 'Debugger.resume' });
  await checkScriptContext(session, childContext);
  await session.waitForBreakOnLine(30, '[eval]');

  console.error('[test]', 'vm.runInNewContext associates script with context');
  await session.send({ 'method': 'Debugger.resume' });
  const thirdContext = await getContext(session);
  await checkScriptContext(session, thirdContext);
  await session.waitForBreakOnLine(33, '[eval]');

  console.error('[test]', 'vm.runInNewContext can contain debugger statements');
  await session.send({ 'method': 'Debugger.resume' });
  const fourthContext = await getContext(session);
  await checkScriptContext(session, fourthContext);
  await session.waitForBreakOnLine(0, 'evalmachine.<anonymous>');

  await session.runToCompletion();
  assert.strictEqual(0, (await instance.expectShutdown()).exitCode);
}

runTests();
