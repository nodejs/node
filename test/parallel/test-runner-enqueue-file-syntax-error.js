'use strict';
require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fixtures = require('../common/fixtures');

const testFile = fixtures.path('test-runner', 'syntax-error-test.mjs');
const child = spawnSync(process.execPath, [
  '--no-warnings',
  '--test',
  '--test-reporter=tap',
  '--test-isolation=none',
  testFile,
], { encoding: 'utf8' });

assert.match(child.stdout, /error:.*"Unexpected token 'true'"\n/);
assert.match(child.stdout, /SyntaxError/);
assert.strictEqual(child.status, 1);
