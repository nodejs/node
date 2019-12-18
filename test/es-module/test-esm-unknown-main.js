'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');

{
  const entry = fixtures.path(
    '/es-modules/package-type-module/extension.unknown'
  );
  const child = spawn(process.execPath, ['--experimental-modules', entry]);
  let stdout = '';
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.ok(stderr.indexOf('ERR_UNKNOWN_FILE_EXTENSION') !== -1);
  }));
}
{
  const entry = fixtures.path(
    '/es-modules/package-type-module/imports-unknownext.mjs'
  );
  const child = spawn(process.execPath, ['--experimental-modules', entry]);
  let stdout = '';
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, '');
    assert.ok(stderr.indexOf('ERR_UNKNOWN_FILE_EXTENSION') !== -1);
  }));
}
{
  const entry = fixtures.path('/es-modules/package-type-module/noext-esm');
  const child = spawn(process.execPath, ['--experimental-modules', entry]);
  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'executed\n');
  }));
}
{
  const entry = fixtures.path(
    '/es-modules/package-type-module/imports-noext.mjs'
  );
  const child = spawn(process.execPath, ['--experimental-modules', entry]);
  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'executed\n');
  }));
}
