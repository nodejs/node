'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');

const requiring = path.resolve(fixtures.path('/es-modules/cjs-esm.js'));
const required = path.resolve(
  fixtures.path('/es-modules/package-type-module/cjs.js')
);
const pjson = path.resolve(
  fixtures.path('/es-modules/package-type-module/package.json')
);

const basename = 'cjs.js';

const child = spawn(process.execPath, [requiring]);
let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);

  assert.ok(stderr.replace(/\r/g, '').includes(
    `Error [ERR_REQUIRE_ESM]: Must use import to load ES Module: ${required}` +
    '\nrequire() of ES modules is not supported.\nrequire() of ' +
    `${required} from ${requiring} ` +
    'is an ES module file as it is a .js file whose nearest parent ' +
    'package.json contains "type": "module" which defines all .js ' +
    'files in that package scope as ES modules.\nInstead rename ' +
    `${basename} to end in .cjs, change the requiring code to use ` +
    'import(), or remove "type": "module" from ' +
    `${pjson}.\n`));
  assert.ok(stderr.includes(
    'Error [ERR_REQUIRE_ESM]: Must use import to load ES Module'));
}));
