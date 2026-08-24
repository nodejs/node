'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const path = require('path');
const { NodeInstance } = require('../common/inspector-helper');

const driver = path.join(__dirname, '../wpt/test-compression.js');

async function main() {
  const parent = new NodeInstance([], `
    delete process.env.WPT_BACKEND;
    process.env.WPT_INSPECT = '1';
    process.argv[2] = 'compression/compression-bad-chunks.any.html';
    require(${JSON.stringify(driver)});
  `, '', {
    log() {},
    error() {},
  });
  const stderr = [];
  parent.on('stderr', (line) => stderr.push(line));

  const session = await parent.connectInspectorSession();
  await session.send([
    { method: 'Runtime.enable' },
    { method: 'Debugger.enable' },
    { method: 'Runtime.runIfWaitingForDebugger' },
  ]);
  await session.waitForNotification('Debugger.paused');
  await session.send({ method: 'Debugger.resume' });
  await session.disconnect();

  const { exitCode, signal } = await parent.expectShutdown();
  assert.strictEqual(signal, null);
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(
    stderr.filter((line) => line.startsWith('Debugger listening on ')).length,
    1,
  );
}

main().then(common.mustCall());
