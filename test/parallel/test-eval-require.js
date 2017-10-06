'use strict';
const common = require('../common');
const { fixturesDir } = require('../common/fixtures');
const assert = require('assert');
const spawn = require('child_process').spawn;

const options = {
  cwd: fixturesDir
};
const child = spawn(process.execPath, ['-e', 'require("foo")'], options);
child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
