'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync, spawn } = require('node:child_process');
const { once } = require('events');
const { finished } = require('stream/promises');

async function runAndKill(file) {
  if (common.isWindows) {
    common.printSkipMessage(`signals are not supported in windows, skipping ${file}`);
    return;
  }
  let stdout = '';
  const child = spawn(process.execPath, ['--test', '--test-reporter=tap', file]);
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => {
    if (!stdout.length) child.kill('SIGINT');
    stdout += chunk;
  });
  const [code, signal] = await once(child, 'exit');
  await finished(child.stdout);
  assert(stdout.startsWith('TAP version 13\n'));
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 1);
}

async function spawnAndKillProgrammatic(childArgs, { code: expectedCode, signal: expectedSignal }) {
  if (common.isWindows) {
    common.printSkipMessage('signals are not supported on windows');
    return;
  }
  const child = spawn(process.execPath, childArgs);
  child.stdout.once('data', () => child.kill('SIGINT'));
  const [code, signal] = await once(child, 'exit');
  assert.strictEqual(signal, expectedSignal);
  assert.strictEqual(code, expectedCode);
}

if (process.argv[2] === 'child') {
  const { test, run } = require('node:test');

  if (process.argv[3] === 'pass') {
    test('passing test', () => {
      assert.strictEqual(true, true);
    });
  } else if (process.argv[3] === 'fail') {
    test('failing test', () => {
      assert.strictEqual(true, false);
    });
  } else if (process.argv[3] === 'run-signal-false') {
    run({ files: [fixtures.path('test-runner', 'never_ending_async.js')] });
    console.log('child started');    
  } else if (process.argv[3] === 'run-signal-true') {
    run({ files: [fixtures.path('test-runner', 'never_ending_async.js')], hookSignal: true });
    console.log('child started');    
  } else assert.fail('unreachable');
} else {
  let child = spawnSync(process.execPath, [__filename, 'child', 'pass']);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);

  child = spawnSync(process.execPath, [
    '--test',
    fixtures.path('test-runner', 'default-behavior', 'subdir', 'subdir_test.js'),
  ]);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);


  child = spawnSync(process.execPath, [
    '--test',
    fixtures.path('test-runner', 'todo_exit_code.js'),
  ]);
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
  const stdout = child.stdout.toString();
  assert.match(stdout, /tests 3/);
  assert.match(stdout, /pass 0/);
  assert.match(stdout, /fail 0/);
  assert.match(stdout, /todo 3/);

  child = spawnSync(process.execPath, [__filename, 'child', 'fail']);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);

  runAndKill(fixtures.path('test-runner', 'never_ending_sync.js')).then(common.mustCall());
  runAndKill(fixtures.path('test-runner', 'never_ending_async.js')).then(common.mustCall());

  (async () => {
    await spawnAndKillProgrammatic(
      [__filename, 'child', 'run-signal-false'],
      { signal: 'SIGINT', code: null },
    );
    await spawnAndKillProgrammatic(
      [__filename, 'child', 'run-signal-true'],
      { signal: null, code: 0 },
    );
  })().then(common.mustCall());
}
