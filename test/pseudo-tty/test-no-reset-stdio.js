// Flags: --no-reset-stdio
'use strict';

const assert = require('assert');
const { spawn, spawnSync } = require('child_process');
const common = require('../common');

function runStty(...args) {
  const result = spawnSync('stty', args, { stdio: 'inherit' });
  assert.strictEqual(result.status, 0);
}

function hasEchoEnabled() {
  const result = spawnSync('stty', ['-a'], { encoding: 'utf8' });
  assert.strictEqual(result.status, 0);
  return !/(^|[\s;])-echo([\s;]|$)/.test(result.stdout);
}

let restored = false;
function restoreTerminal() {
  if (!restored) {
    restored = true;
    runStty('echo');
  }
}

process.on('exit', restoreTerminal);

runStty('echo');

const child = spawn(process.execPath, [
  '--no-reset-stdio',
  '-e',
  "process.send('ready'); setTimeout(() => {}, 1000)",
], {
  stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
});

child.once('message', common.mustCall((message) => {
  assert.strictEqual(message, 'ready');
  runStty('-echo');
}));

child.once('close', common.mustCall((code, signal) => {
  try {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(hasEchoEnabled(), false);
    console.log('ok');
  } finally {
    restoreTerminal();
  }
}));
