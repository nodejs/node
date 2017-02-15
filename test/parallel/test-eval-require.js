'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const options = {
  cwd: common.fixturesDir
};
const child = spawn(process.execPath, ['-e', 'require("foo")'], options);
child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
