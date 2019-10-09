'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');

const entry = fixtures.path('/es-modules/cjs-esm.js');

const child = spawn(process.execPath, [entry]);
let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(stderr, `(node:${child.pid}) Warning: require() of .js ` +
      `file ${path.resolve(entry, '../package-type-module/cjs.js')} is not ` +
      'supported as it is an ES module due to having "type": "module" in its ' +
      'package.json file.\nRather use import to load this module, or if you ' +
      'are the author you may want to use the .cjs extension for this file.\n');
}));
