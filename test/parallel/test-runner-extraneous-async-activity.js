'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync } = require('child_process');

{
  const child = spawnSync(process.execPath, [
    '--test',
    fixtures.path('test-runner', 'extraneous_set_immediate_async.mjs'),
  ]);
  const stdout = child.stdout.toString();
  assert.match(stdout, /^# Warning: Test "extraneous async activity test" generated asynchronous activity after the test ended/m);
  assert.match(stdout, /^# pass 1/m);
  assert.match(stdout, /^# fail 1$/m);
  assert.match(stdout, /^# cancelled 0$/m);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
}

{
  const child = spawnSync(process.execPath, [
    '--test',
    fixtures.path('test-runner', 'extraneous_set_timeout_async.mjs'),
  ]);
  const stdout = child.stdout.toString();
  assert.match(stdout, /^# Warning: Test "extraneous async activity test" generated asynchronous activity after the test ended/m);
  assert.match(stdout, /^# pass 1$/m);
  assert.match(stdout, /^# fail 1$/m);
  assert.match(stdout, /^# cancelled 0$/m);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
}
