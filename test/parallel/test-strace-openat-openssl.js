'use strict';

const common = require('../common');
const { spawn, spawnSync } = require('node:child_process');
const { createInterface } = require('node:readline');
const assert = require('node:assert');

if (!common.hasCrypto)
  common.skip('missing crypto');
if (!common.isLinux)
  common.skip('linux only');
if (common.isASan)
  common.skip('strace does not work well with address sanitizer builds');
if (spawnSync('strace').error !== undefined) {
  common.skip('missing strace');
}

{
  const allowedOpenCalls = new Set([
    '/etc/ssl/openssl.cnf',
  ]);
  const syscalls = ['openat'];
  if (process.arch !== 'riscv64' && process.arch !== 'riscv32') {
    syscalls.push('open');
  }
  const strace = spawn('strace', [
    '-f', '-ff',
    '-e', `trace=${syscalls.join(',')}`,
    '-s', '512',
    '-D', process.execPath, '-e', 'require("crypto")',
  ]);

  // stderr is the default for strace
  const rl = createInterface({ input: strace.stderr });
  rl.on('line', common.mustCallAtLeast((line) => {
    if (!line.startsWith('open')) {
      return;
    }

    const file = line.match(/"(.*?)"/)[1];
    // skip .so reading attempt
    if (file.match(/.+\.so(\.?)/) !== null) {
      return;
    }
    // skip /proc/*
    if (file.match(/\/proc\/.+/) !== null) {
      return;
    }

    assert(allowedOpenCalls.delete(file), `${file} is not in the list of allowed openat calls`);
  }));
  const debugOutput = [];
  strace.stderr.setEncoding('utf8');
  strace.stderr.on('data', (chunk) => {
    debugOutput.push(chunk.toString());
  });
  strace.on('error', common.mustNotCall());
  strace.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0, debugOutput);
    const missingKeys = Array.from(allowedOpenCalls.keys());
    if (missingKeys.length) {
      assert.fail(`The following openat call are missing: ${missingKeys.join(',')}`);
    }
  }));
}
