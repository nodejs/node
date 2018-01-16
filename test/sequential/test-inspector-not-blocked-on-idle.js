// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const { NodeInstance } = require('../common/inspector-helper.js');

async function runTests() {
  const script = 'setInterval(() => {debugger;}, 60000);';
  const node = new NodeInstance('--inspect=0', script);
  // 1 second wait to make sure the inferior began running the script
  await new Promise((resolve) => setTimeout(() => resolve(), 1000));
  const session = await node.connectInspectorSession();
  await session.send([
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.pause' }
  ]);
  session.disconnect();
  node.kill();
}

common.crashOnUnhandledRejection();
runTests();
