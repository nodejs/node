'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { strictEqual } = require('assert');
const { closeSync, openSync, readFileSync, writeFileSync } = require('fs');
const { join } = require('path');
const { WASI } = require('wasi');
const modulePath = join(__dirname, 'wasm', 'stdin.wasm');
const buffer = readFileSync(modulePath);
const stdinFile = tmpdir.resolve('stdin.txt');
const stdoutFile = tmpdir.resolve('stdout.txt');
const stderrFile = tmpdir.resolve('stderr.txt');

tmpdir.refresh();
// Write 33 x's. The test's buffer only holds 31 x's + a terminator.
writeFileSync(stdinFile, 'x'.repeat(33));

const stdin = openSync(stdinFile, 'r');
const stdout = openSync(stdoutFile, 'a');
const stderr = openSync(stderrFile, 'a');
const wasi = new WASI({ version: 'preview1', stdin, stdout, stderr, returnOnExit: true });
const importObject = { wasi_snapshot_preview1: wasi.wasiImport };

(async () => {
  const { instance } = await WebAssembly.instantiate(buffer, importObject);

  strictEqual(wasi.start(instance), 0);
  closeSync(stdin);
  closeSync(stdout);
  closeSync(stderr);
  strictEqual(readFileSync(stdoutFile, 'utf8').trim(), 'x'.repeat(31));
  strictEqual(readFileSync(stderrFile, 'utf8').trim(), '');
})().then(common.mustCall());
