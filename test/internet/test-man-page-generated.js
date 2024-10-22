'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');
const { readFileSync } = require('node:fs');
const path = require('node:path');

test('Verify that the man-page has been properly generated', async (t) => {
  tmpdir.refresh();

  const root = path.resolve(__dirname, '..', '..');
  const expected = tmpdir.resolve('node.1');

  await common.spawnPromisified(process.execPath, [
    path.join(root, 'deps', 'npm', 'bin', 'npx-cli.js'),
    '--yes',
    'github:nodejs/api-docs-tooling',
    '-i', path.join(root, 'doc', 'api', 'cli.md'),
    '-o', expected,
    '-t', 'man-page',
  ]);

  t.assert.strictEqual(
    readFileSync(path.join(root, 'doc', 'node.1'), 'utf-8'),
    readFileSync(expected, 'utf-8'),
  );
});
