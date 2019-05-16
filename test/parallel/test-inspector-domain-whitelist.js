// Flags: --expose-internals
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');
const script = `
const session = new (require('inspector').Session)();
session.connect();
session.post('Debugger.enable', (error, res) => {
  if (!error) {
    process.exit(42);
  }
});`;

/**
 * This test verifies that it is possible to disable built-in V8 inpsector
 * domains by providing a domains whitelist from the command line.
 * Disabled domains are still available for "trusted" sessions (ones that are
 * not accepting arbitrary commands from external clients)
 */
async function runTest() {
  const child = new NodeInstance([
    '--inspect-brk=0', '--inspector-domain-whitelist=Runtime', '-e', script
  ]);
  const session = await child.connectInspectorSession();
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  try {
    const response = await session.send({ method: 'Debugger.enable' });
    assert.fail(JSON.stringify(response));
  } catch (e) {
    console.log(`Expected error ${JSON.stringify(e)} was recieved`);
  }
  await session.disconnect();
  assert.strictEqual((await child.expectShutdown()).exitCode, 42);
}

runTest();
