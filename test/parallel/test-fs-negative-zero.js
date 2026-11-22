'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');

tmpdir.refresh();

const missing = tmpdir.resolve(
  `node-fs-negative-zero-${process.pid}`,
  'entry',
);

function ignoreExpectedError(fn) {
  try {
    fn();
  } catch {
    // Ignore expected file system errors from the missing path.
  }
}

const fd = fs.openSync(process.execPath, -0);
fs.closeSync(fd);

ignoreExpectedError(() => fs.openSync(process.execPath, 'r', -0));
ignoreExpectedError(() => fs.readFileSync(process.execPath, { flag: -0 }));
ignoreExpectedError(() => fs.mkdirSync(missing, { mode: -0 }));
ignoreExpectedError(() => fs.chmodSync(missing, -0));
ignoreExpectedError(() => fs.writeFileSync(missing, '', { mode: -0 }));

// -0 is accepted as file descriptor 0. Writing an empty string reaches the
// utf8 fast path without issuing a write on the descriptor.
fs.writeFileSync(-0, '');
fs.appendFileSync(-0, '');

const child = spawnSync(
  process.execPath,
  ['-e', 'process.stdout.write(require("fs").readFileSync(-0, "utf8"))'],
  { input: 'hello' },
);
assert.strictEqual(child.status, 0);
assert.strictEqual(child.stdout.toString(), 'hello');

fs.watchFile(missing, { interval: -0 }, () => {});
fs.unwatchFile(missing);

// A length of -0 writes nothing, like 0.
{
  const buffer = Buffer.from('abc');

  const fd = fs.openSync(tmpdir.resolve('negative-zero-fd.txt'), 'w');
  assert.strictEqual(fs.writeSync(fd, buffer, 0, -0), 0);
  assert.strictEqual(fs.writeSync(fd, buffer, { length: -0 }), 0);
  fs.write(fd, buffer, 0, -0, common.mustSucceed((written) => {
    assert.strictEqual(written, 0);
    fs.write(fd, buffer, { length: -0 }, common.mustSucceed((written) => {
      assert.strictEqual(written, 0);
      fs.closeSync(fd);
    }));
  }));

  (async () => {
    const handle =
      await fs.promises.open(tmpdir.resolve('negative-zero-handle.txt'), 'w');
    assert.strictEqual((await handle.write(buffer, 0, -0)).bytesWritten, 0);
    assert.strictEqual(
      (await handle.write(buffer, { length: -0 })).bytesWritten, 0);
    await handle.close();
  })().then(common.mustCall());
}
