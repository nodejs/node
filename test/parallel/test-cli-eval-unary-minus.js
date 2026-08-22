'use strict';

// Regression test for https://github.com/nodejs/node/issues/43397
//
// `node -pe '-0'` (and other expressions whose first character is `-`) used to
// fail with "--eval requires an argument" because the option parser treated
// the value as another flag. The supported way to pass such values is to
// terminate the option list with `--`, e.g. `node -pe -- -0`.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

function run(...args) {
  const result = spawnSync(process.execPath, args, { encoding: 'utf8' });
  return { stdout: result.stdout, stderr: result.stderr, status: result.status };
}

// `--` should let the next argv entry be consumed verbatim as the value of
// `--eval`, even when it starts with `-`.
{
  const { stdout, stderr, status } = run('-pe', '--', '-0');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '0\n');
}

{
  const { stdout, stderr, status } = run('-pe', '--', '-1.5');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '-1.5\n');
}

{
  const { stdout, stderr, status } = run('-pe', '--', '-1+0');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '-1\n');
}

// `-e` (no print) should also accept a leading-dash value via `--`.
{
  const { stdout, stderr, status } =
    run('-e', '--', '-42; console.log("ok")');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, 'ok\n');
}

// The long-form `--eval` should behave the same way.
{
  const { stdout, stderr, status } = run('--print', '--eval', '--', '-7');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '-7\n');
}

// `--eval=-42` already worked and must keep working.
{
  const { stdout, stderr, status } = run('--print', '--eval=-42');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '-42\n');
}

// The pre-existing `\-` escape must keep working.
{
  const { stdout, stderr, status } = run('-pe', '\\-0');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '0\n');
}

// Without `--`, a leading-dash value still produces the existing diagnostic;
// that behavior is intentional so unrelated stacked flags keep being detected.
{
  const { stderr, status } = run('-pe', '-0');
  assert.notStrictEqual(status, 0);
  assert.match(stderr, /requires an argument/);
}

// Sanity: a positional value with no leading dash works without `--`.
{
  const { stdout, stderr, status } = run('-pe', '42');
  assert.strictEqual(status, 0, `stderr: ${stderr}`);
  assert.strictEqual(stderr, '');
  assert.strictEqual(stdout, '42\n');
}
