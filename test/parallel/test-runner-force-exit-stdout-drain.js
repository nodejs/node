'use strict';
// With --test-force-exit, buffered reporter output on stdout (both in test
// child processes and in the main runner) must be flushed before exiting.
// Refs: https://github.com/nodejs/node/issues/67149
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { writeFileSync } = require('node:fs');
const { once } = require('node:events');
const { setTimeout } = require('node:timers/promises');
const { test } = require('node:test');

const kFiles = 8;
const kTestsPerFile = 300;

tmpdir.refresh();
const files = [];
for (let i = 0; i < kFiles; i++) {
  const file = tmpdir.resolve(`t${i}.test.js`);
  writeFileSync(file, `'use strict';
const { test } = require('node:test');
for (let i = 0; i < ${kTestsPerFile}; i++) {
  test('test ' + i + ' ' + 'x'.repeat(1000), () => {});
}
`);
  files.push(file);
}

test('no results are lost when stdout is read slowly', async () => {
  const child = spawn(process.execPath, [
    '--test',
    '--test-force-exit',
    `--test-concurrency=${kFiles}`,
    '--test-reporter=junit',
    ...files,
  ], { cwd: tmpdir.path });

  // Let the pipes back up before reading anything.
  child.stdout.pause();
  await setTimeout(1000);
  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => { stdout += chunk; });
  child.stdout.resume();

  const [code] = await once(child, 'close');
  assert.strictEqual(code, 0);
  assert.strictEqual(stdout.match(/<testcase /g)?.length, kFiles * kTestsPerFile);
  assert.match(stdout, /<\/testsuites>\n$/);
});
