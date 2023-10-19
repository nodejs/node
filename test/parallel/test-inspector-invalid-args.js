'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

common.skipIfInspectorDisabled();

const assert = require('assert');
const execFile = require('child_process').execFile;

const mainScript = fixtures.path('loop.js');
const expected =
  '`node --debug` and `node --debug-brk` are invalid. ' +
  'Please use `node --inspect` and `node --inspect-brk` instead.';
for (const invalidArg of ['--debug-brk', '--debug']) {
  execFile(
    process.execPath,
    [invalidArg, mainScript],
    common.mustCall((error, stdout, stderr) => {
      assert.strictEqual(error.code, 9, `node ${invalidArg} should exit 9`);
      assert.strictEqual(
        stderr.includes(expected),
        true,
        `${stderr} should include '${expected}'`
      );
    })
  );
}
