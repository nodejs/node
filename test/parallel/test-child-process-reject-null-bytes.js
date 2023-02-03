'use strict';
const { mustNotCall } = require('../common');

// Regression test for https://github.com/nodejs/node/issues/44768

const { throws } = require('assert');
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

throws(() => exec(`${process.execPath} ${__filename} AAA BBB\0XXX CCC`, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'command' must be a string without null bytes/
});

throws(() => exec('BBB\0XXX AAA CCC', mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'command' must be a string without null bytes/
});

throws(() => execSync(`${process.execPath} ${__filename} AAA BBB\0XXX CCC`), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'command' must be a string without null bytes/
});

throws(() => execSync('BBB\0XXX AAA CCC'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'command' must be a string without null bytes/
});

// Tests for the 'file' argument

throws(() => spawn('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'file' must be a string without null bytes/
});

throws(() => execFile('BBB\0XXX', mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'file' must be a string without null bytes/
});

throws(() => execFileSync('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'file' must be a string without null bytes/
});

throws(() => spawn('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'file' must be a string without null bytes/
});

throws(() => spawnSync('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'file' must be a string without null bytes/
});

// Tests for the 'modulePath' argument

throws(() => fork('BBB\0XXX'), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'modulePath' must be a string or Uint8Array without null bytes/
});

// Tests for the 'args' argument

// Not testing exec() and execSync() because these accept 'args' as a part of
// 'command' as space-separated arguments.

throws(() => execFile(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC'], mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'args\[2\]' must be a string without null bytes/
});

throws(() => execFileSync(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'args\[2\]' must be a string without null bytes/
});

throws(() => fork(__filename, ['AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'args\[2\]' must be a string without null bytes/
});

throws(() => spawn(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'args\[2\]' must be a string without null bytes/
});

throws(() => spawnSync(process.execPath, [__filename, 'AAA', 'BBB\0XXX', 'CCC']), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The argument 'args\[2\]' must be a string without null bytes/
});

// Tests for the 'options.cwd' argument

throws(() => exec(process.execPath, { cwd: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => execFile(process.execPath, { cwd: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => execFileSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => execSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => fork(__filename, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => spawn(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

throws(() => spawnSync(process.execPath, { cwd: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.cwd' must be a string or Uint8Array without null bytes/
});

// Tests for the 'options.argv0' argument

throws(() => exec(process.execPath, { argv0: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => execFile(process.execPath, { argv0: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => execFileSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => execSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => fork(__filename, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => spawn(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

throws(() => spawnSync(process.execPath, { argv0: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.argv0' must be a string without null bytes/
});

// Tests for the 'options.shell' argument

throws(() => exec(process.execPath, { shell: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

throws(() => execFile(process.execPath, { shell: 'BBB\0XXX' }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

throws(() => execFileSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

throws(() => execSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

// Not testing fork() because it doesn't accept the shell option (internally it
// explicitly sets shell to false).

throws(() => spawn(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

throws(() => spawnSync(process.execPath, { shell: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.shell' must be a string without null bytes/
});

// Tests for the 'options.env' argument

throws(() => exec(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => exec(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => execFile(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => execFile(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }, mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => execFileSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => execFileSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => execSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => execSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => fork(__filename, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => fork(__filename, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => spawn(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => spawn(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

throws(() => spawnSync(process.execPath, { env: { 'AAA': 'BBB\0XXX' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['AAA'\]' must be a string without null bytes/
});

throws(() => spawnSync(process.execPath, { env: { 'BBB\0XXX': 'AAA' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.env\['BBB\0XXX'\]' must be a string without null bytes/
});

// Tests for the 'options.execPath' argument
throws(() => fork(__filename, { execPath: 'BBB\0XXX' }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.execPath' must be a string without null bytes/
});

// Tests for the 'options.execArgv' argument
throws(() => fork(__filename, { execArgv: ['AAA', 'BBB\0XXX', 'CCC'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.execArgv\[1\]' must be a string without null bytes/
});
