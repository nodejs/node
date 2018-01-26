'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (common.isWindows)
  common.skip('Sending signals is not supported');

if (process.argv[2] === 'child') {
  if (process.argv[3] === 'once') {
    const on_sigint = () => {};
    process.on('SIGINT', on_sigint);
    process.once('SIGINT', () => {});
    process.removeListener('SIGINT', on_sigint);
    process.once('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'signal');
      process.send('signal');
      while (true) {}
    }));
  } else if (process.argv[3] === 'on') {
    let signals = 0;
    process.once('SIGINT', common.mustCall(() => {
      process.send('signal');
    }));

    process.on('SIGINT', common.mustCall(() => {
      if (++signals === 2)
        process.exit(0);
    }, 2));
  }

  process.send('signal');
  setTimeout(() => {}, 100000);

  return;
}

{
  const child = spawn(process.execPath, [__filename, 'child', 'once'], {
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGINT');
  }));

  child.on('message', (msg) => {
    assert.strictEqual(msg, 'signal');
    child.kill('SIGINT');
    child.send('signal');
  });
}

{
  const child = spawn(process.execPath, [__filename, 'child', 'on'], {
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));

  child.on('message', (msg) => {
    assert.strictEqual(msg, 'signal');
    child.kill('SIGINT');
  });
}
