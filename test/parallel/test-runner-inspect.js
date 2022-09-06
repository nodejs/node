'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir.js');
const { NodeInstance } = require('../common/inspector-helper.js');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');
const testFixtures = fixtures.path('test-runner');

common.skipIfInspectorDisabled();
tmpdir.refresh();

async function debugTest() {
  const child = new NodeInstance(['--test', '--inspect-brk=0'], undefined, join(testFixtures, 'index.test.js'));

  let stdout = '';
  let stderr = '';
  child.on('stdout', (line) => stdout += line);
  child.on('stderr', (line) => stderr += line);

  const session = await child.connectInspectorSession();

  await session.send([
    { method: 'Runtime.enable' },
    { method: 'NodeRuntime.notifyWhenWaitingForDisconnect',
      params: { enabled: true } },
    { method: 'Runtime.runIfWaitingForDebugger' }]);


  await session.waitForNotification((notification) => {
    return notification.method === 'NodeRuntime.waitingForDisconnect';
  });

  session.disconnect();
  assert.match(stderr,
               /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
}

debugTest().then(common.mustCall());


{
  const args = ['--test', '--inspect=0', join(testFixtures, 'index.js')];
  const child = spawnSync(process.execPath, args);

  assert.match(child.stderr.toString(),
               /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
  const stdout = child.stdout.toString();
  assert.match(stdout, /not ok 1 - .+index\.js/);
  assert.match(stdout, /stderr: \|-\r?\n\s+Debugger listening on/);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
}


{
  // File not found.
  const args = ['--test', '--inspect=0', 'a-random-file-that-does-not-exist.js'];
  const child = spawnSync(process.execPath, args);

  const stderr = child.stderr.toString();
  assert.strictEqual(child.stdout.toString(), '');
  assert.match(stderr, /^Could not find/);
  assert.doesNotMatch(stderr, /Warning: Using the inspector with --test forces running at a concurrency of 1\. Use the inspectPort option to run with concurrency/);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
}
