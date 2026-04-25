'use strict';
const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('registering customization hooks in Workers does not work');
}

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { test } = require('node:test');

// Regression test for https://github.com/nodejs/node/issues/58231
// When a dual package exposes both ESM and CJS entry points via the
// "exports" field with "import"/"require" conditions, the ESM resolver
// picks one file (e.g. index.js) and CJS require() picks another
// (e.g. index.cjs). mock.module() must intercept both so that require()
// of the mocked module does not return the original CJS file.
test('mock.module intercepts dual package require with conditional exports',
     async () => {
       const cwd = fixtures.path('test-runner');
       const fixture = fixtures.path('test-runner', 'mock-nm-dual-pkg.js');
       const args = ['--experimental-test-module-mocks', fixture];
       const {
         code,
         stdout,
         signal,
       } = await common.spawnPromisified(process.execPath, args, { cwd });

       assert.strictEqual(signal, null);
       assert.strictEqual(code, 0,
                          'child process exited with non-zero status\n' +
                          `stdout:\n${stdout}`);
       assert.match(stdout, /pass 1/);
       assert.match(stdout, /fail 0/);
     });
