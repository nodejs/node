'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const testFixture = fixtures.path('test-runner/argv.js');

{
  const args = ['--test', testFixture, '--hello'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /"--hello"\]/);
  assert.strictEqual(child.status, 0);
}

{
  const args = ['--test', testFixture, '--', '--hello', '--world'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /"--","--hello","--world"\]/);
  assert.strictEqual(child.status, 0);
}

{
  const args = ['--test', testFixture, '--', 'foo.js'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /"--","foo\.js"\]/);
  assert.strictEqual(child.status, 0);
}

{
  const args = ['--test', testFixture, '--hello', '--', 'world'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /"--hello","--","world"\]/);
  assert.strictEqual(child.status, 0);
}
