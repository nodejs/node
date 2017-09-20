'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const native = `${common.fixturesDir}/es-module-url/native.mjs`;
const child = spawn(process.execPath, ['--experimental-modules', native]);
child.on('exit', (code) => {
  assert.strictEqual(code, 1);
});
