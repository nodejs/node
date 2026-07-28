'use strict';
const { mustNotCall } = require('../common');

// Regression test for https://github.com/nodejs/node/issues/44768

const assert = require('assert');
const {
  exec,
  execFile,
  execFileSync,
  execSync,
  fork,
  spawn,
  spawnSync,
} = require('child_process');

// Tests for the 'command' argument

assert.throws(() => exec(`${process.execPath} ${__filename} AAA BBB\0XXX CCC`, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => exec('BBB\0XXX AAA CCC', mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(`${process.execPath} ${__filename} AAA BBB\0XXX CCC`), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync('BBB\0XXX AAA CCC'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'file' argument

assert.throws(() => spawn('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile('BBB\0XXX', mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'modulePath' argument

assert.throws(() => fork('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'args' argument

// Not testing exec() and execSync() because these accept 'args' as a part of
// 'command' as space-separated arguments.

assert.throws(() => execFile(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC'], mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => fork(__filename, ['AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.cwd' argument

assert.throws(() => exec(process.execPath, { cwd: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile(process.execPath, { cwd: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => fork(__filename, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.argv0' argument

assert.throws(() => exec(process.execPath, { argv0: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile(process.execPath, { argv0: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => fork(__filename, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.shell' argument

assert.throws(() => exec(process.execPath, { shell: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile(process.execPath, { shell: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Not testing fork() because it doesn't accept the shell option (internally it
// explicitly sets shell to false).

assert.throws(() => spawn(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.env' argument

assert.throws(() => exec(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => exec(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFile(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execFileSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => execSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => fork(__filename, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => fork(__filename, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawn(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

assert.throws(() => spawnSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.execPath' argument
assert.throws(() => fork(__filename, { execPath: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});

// Tests for the 'options.execArgv' argument
assert.throws(() => fork(__filename, { execArgv: ['AAA', 'BBB\0XXX', 'CCC'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
});
