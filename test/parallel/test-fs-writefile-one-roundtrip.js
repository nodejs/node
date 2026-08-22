'use strict';
// fs.writeFile() and fs.promises.writeFile() with a path perform
// open + write + close as one thread pool request. This covers what that
// request must keep doing: honor flags and mode, append, report the
// failing syscall, accept every ArrayBufferView, and write buffers larger
// than one write() call in full.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();
let counter = 0;
const next = () => tmpdir.resolve(`file-${counter++}`);

async function check(write) {
  {
    const file = next();
    await write(file, 'hello');
    await write(file, ' world', { flag: 'a' });
    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'hello world');
    await assert.rejects(write(file, 'again', { flag: 'wx' }), { code: 'EEXIST', syscall: 'open', path: file });
  }
  {
    const file = path.join(next(), 'missing-dir', 'file');
    await assert.rejects(write(file, 'x'), { code: 'ENOENT', syscall: 'open', path: file });
  }
  {
    const file = next();
    await write(file, '');
    assert.strictEqual(fs.statSync(file).size, 0);
  }
  if (!common.isWindows) {
    const file = next();
    const mask = process.umask(0o022);
    await write(file, 'x', { mode: 0o640 });
    process.umask(mask);
    assert.strictEqual(fs.statSync(file).mode & 0o777, 0o640);
  }
  {
    const file = next();
    const units = new Uint16Array([0x6968]);
    await write(file, units);
    await write(file, new DataView(new TextEncoder().encode('!?').buffer, 1, 1), { flag: 'a' });
    assert.deepStrictEqual(fs.readFileSync(file),
                           Buffer.concat([Buffer.from(units.buffer), Buffer.from('?')]));
  }
  if (fs.existsSync('/dev/full')) {
    // open() succeeds, write() fails.
    await assert.rejects(write('/dev/full', 'x'), { code: 'ENOSPC', syscall: 'write' });
  }
  {
    const file = next();
    const big = Buffer.alloc(3 * 1024 * 1024 + 7, 'z');
    await write(file, big);
    assert.deepStrictEqual(fs.readFileSync(file), big);
  }
}

(async () => {
  await check((file, data, options) => new Promise((resolve, reject) => {
    fs.writeFile(file, data, options, (err) => (err ? reject(err) : resolve()));
  }));
  await check(fs.promises.writeFile);
})().then(common.mustCall());
