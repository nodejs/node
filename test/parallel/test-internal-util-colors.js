'use strict';

const common = require('../common');
const { test } = require('node:test');

test('should colorize when FORCE_COLOR, even in a piped stdio', async (t) => {
  const script =
    'const colors = require(\'internal/util/colors\');' +
    'require(\'assert\')(colors.hasColors === colors.shouldColorize());';
  const child = await common.spawnPromisified(process.execPath, [
    '--expose-internals',
    '-e', script,
  ], {
    env: { FORCE_COLOR: 1 },
    stdio: 'pipe'
  });

  t.assert.strictEqual(child.stderr, '');
  t.assert.strictEqual(child.code, 0);
});
