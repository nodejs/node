'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const execFile = require('child_process').execFile;
const path = require('path');

const mainScript = path.join(common.fixturesDir, 'loop.js');
const expected =
  '`node --debug` and `node --debug-brk` are invalid. ' +
  'Please use `node --inspect` or `node --inspect-brk` instead.';
for (const invalidArg of ['--debug-brk', '--debug']) {
  execFile(
    process.execPath,
    [ invalidArg, mainScript ],
    common.mustCall((error, stdout, stderr) => {
      assert.strictEqual(error.code, 9, `node ${invalidArg} should exit 9`);
      assert.strictEqual(stderr.includes(expected), true,
                         `${stderr} should include '${expected}'`);
    })
  );
}
