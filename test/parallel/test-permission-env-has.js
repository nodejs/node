// Flags: --experimental-permission --allow-env=* --allow-fs-read=* --allow-child-process
'use strict';

require('../common');
const { spawnSync } = require('node:child_process');
const { debuglog } = require('node:util');
const { strictEqual, deepStrictEqual } = require('node:assert');
const { describe, it } = require('node:test');

const debug = debuglog('test');

function runTest(args, options = {}) {
  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    ['--experimental-permission', '--allow-fs-read=*', ...args],
    options
  );
  debug('status:', status);
  if (status) debug('stdout:', stdout.toString().split('\n'));
  if (status) debug('stderr:', stderr.toString().split('\n'));
  return { status, stdout, stderr };
}

describe('permission: has "env" with reference', () => {
  const code = `
  const has = (...args) => console.log(process.permission.has(...args));

  has('env', 'A1');
  has('env', 'A2');
  has('env', 'B1');
  has('env', 'B2');
  has('env', 'NODE_OPTIONS');
  `;

  it('allow one', () => {
    const { status, stdout } = runTest(['--allow-env=A1', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'false',
      'false',
      'false',
      'false',
    ]);
  });

  it('allow more than one', () => {
    const { status, stdout } = runTest(['--allow-env=A1,A2', '-e', code]);

    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'false',
    ]);
  });

  it('allow more than one using wildcard', () => {
    const { status, stdout } = runTest(['--allow-env=A*', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'false',
    ]);
  });

  it('allow more than one with spaces', () => {
    const { status, stdout } = runTest(['--allow-env=A1,   A2   ', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'false',
    ]);
  });

  it('allow all', () => {
    const { status, stdout } = runTest(['--allow-env=*', '-e', code]);

    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'true',
      'true',
      'true',
    ]);
  });

  it('allow all except one', () => {
    const { status, stdout } = runTest(['--allow-env=*,-B1', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'true',
      'true',
    ]);
  });

  it('allow all except more than one', () => {
    const { status, stdout } = runTest(['--allow-env=*,-B1,-B2', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'true',
    ]);
  });

  it('allow all except more than one using wildcard', () => {
    const { status, stdout } = runTest(['--allow-env=*,-B*', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'true',
    ]);
  });

  it('allow all except more than one using wildcard (reverse order)', () => {
    const { status, stdout } = runTest(['--allow-env=-B*,*', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'true',
      'true',
      'false',
      'false',
      'true',
    ]);
  });

  it('deny one', () => {
    const { status, stdout } = runTest(['--allow-env=-B1', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'false',
      'false',
      'false',
      'false',
      'false',
    ]);
  });

  it('deny all', () => {
    const { status, stdout } = runTest(['--allow-env=-*', '-e', code]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'false',
      'false',
      'false',
      'false',
      'false',
    ]);
  });
});

describe('permission: has "env" with no reference', () => {
  it('initial state', () => {
    const { status } = runTest([
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), false);',
    ]);
    strictEqual(status, 0);
  });

  it('has no reference with *', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), true);',
    ]);
    strictEqual(status, 0);
  });

  it('has no reference with A*', () => {
    const { status } = runTest([
      '--allow-env=A*',
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), false);',
    ]);
    strictEqual(status, 0);
  });

  it('has no reference with *,-A', () => {
    const { status } = runTest([
      '--allow-env=*,-A',
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), false);',
    ]);
    strictEqual(status, 0);
  });

  it('has no reference with *,-A*', () => {
    const { status } = runTest([
      '--allow-env=*,-A*',
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), false);',
    ]);
    strictEqual(status, 0);
  });

  it('has no reference with -*,*', () => {
    const { status } = runTest([
      '--allow-env=-*,*',
      '-e',
      'require("assert").strictEqual(process.permission.has("env"), false);',
    ]);
    strictEqual(status, 0);
  });
});

describe('permission: reference', () => {
  it('reference is case-sensitive', () => {
    const { status, stdout } = runTest([
      '--allow-env=FoO',
      '-e',
      `
      console.log(process.permission.has('env', 'FOO'));
      console.log(process.permission.has('env', 'foo'));
      console.log(process.permission.has('env', 'FoO'));
      `,
    ]);
    strictEqual(status, 0);
    deepStrictEqual(stdout.toString().split('\n').slice(0, -1), [
      'false',
      'false',
      'true',
    ]);
  });
});
