'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawn } = require('child_process');

const native = fixtures.path('es-module-url/native.mjs');
const child = spawn(process.execPath, [native]);
child.on('exit', (code) => {
  assert.strictEqual(code, 1);
});
