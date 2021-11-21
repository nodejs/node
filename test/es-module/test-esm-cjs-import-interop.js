'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');

const entry = fixtures.path(
  '/es-modules/package-cjs-import-interop/cjs-exports.mjs'
);

const child = spawn(process.execPath, [entry]);
child.stderr.setEncoding('utf8');
let stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  stdout += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdout, 'ok\n');
}));

const entryCjs = fixtures.path(
  '/es-modules/package-cjs-import-interop/cjs-exports-dynamic.cjs'
);

const childCjs = spawn(process.execPath, [entryCjs]);
childCjs.stderr.setEncoding('utf8');
let stdoutCjs = '';
childCjs.stdout.setEncoding('utf8');
childCjs.stdout.on('data', (data) => {
  stdoutCjs += data;
});
childCjs.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdoutCjs, 'ok\n');
}));
