'use strict';

const { mustCall, isWindows } = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const { strictEqual, ok } = require('assert');

const entry = fixtures.path('/es-modules/import-invalid-pjson.mjs');
const invalidJson = fixtures.path('/node_modules/invalid-pjson/package.json');

const child = spawn(process.execPath, [entry]);
child.stderr.setEncoding('utf8');
let stderr = '';
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', mustCall((code, signal) => {
  strictEqual(code, 1);
  strictEqual(signal, null);
  ok(
    stderr.includes(
      `[ERR_INVALID_PACKAGE_CONFIG]: Invalid package config ${invalidJson} ` +
      `while importing "invalid-pjson" from ${entry}. ` +
      `Unexpected token } in JSON at position ${isWindows ? 16 : 14}`
    ),
    stderr);
}));
