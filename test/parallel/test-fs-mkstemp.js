'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function checkPath(file, prefix) {
  const name = path.basename(file);
  assert.strictEqual(path.dirname(file), tmpdir.path);
  assert.strictEqual(name.length, prefix.length + 6);
  assert.ok(name.startsWith(prefix));
}

function checkFile({ path: file, fd }, prefix) {
  checkPath(file, prefix);
  assert.strictEqual(fs.writeSync(fd, 'hello'), 5);
  if (!common.isWindows) {
    assert.strictEqual(fs.fstatSync(fd).mode & 0o777, 0o600);
  }
  fs.closeSync(fd);
  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'hello');
}

// Sync
{
  const first = fs.mkstempSync(tmpdir.resolve('sync-'));
  const second = fs.mkstempSync(tmpdir.resolve('sync-'));
  assert.notStrictEqual(first.path, second.path);
  checkFile(first, 'sync-');
  checkFile(second, 'sync-');

  checkFile(fs.mkstempSync(pathToFileURL(tmpdir.resolve('url-'))), 'url-');

  for (const result of [
    fs.mkstempSync(tmpdir.resolve('buffer-'), 'buffer'),
    fs.mkstempSync(tmpdir.resolve('buffer-'), { encoding: 'buffer' }),
    fs.mkstempSync(Buffer.from(tmpdir.resolve('buffer-'))),
  ]) {
    assert.ok(Buffer.isBuffer(result.path));
    checkFile({ path: result.path.toString(), fd: result.fd }, 'buffer-');
  }
}

// Callback
{
  fs.mkstemp(tmpdir.resolve('callback-'), common.mustSucceed((file) => {
    checkFile(file, 'callback-');
  }));

  fs.mkstemp(tmpdir.resolve('callback-'), 'buffer', common.mustSucceed((file) => {
    assert.ok(Buffer.isBuffer(file.path));
    checkFile({ path: file.path.toString(), fd: file.fd }, 'callback-');
  }));
}

// Promises
(async () => {
  const { path: file, handle } = await fs.promises.mkstemp(tmpdir.resolve('promise-'));
  checkPath(file, 'promise-');
  await handle.writeFile('hello');
  await handle.close();
  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'hello');

  const result = await fs.promises.mkstemp(Buffer.from(tmpdir.resolve('promise-')));
  assert.ok(Buffer.isBuffer(result.path));
  await result.handle.close();
})().then(common.mustCall());

// Errors report the template.
{
  const prefix = tmpdir.resolve('missing', 'error-');
  const expected = {
    code: 'ENOENT',
    syscall: 'mkstemp',
    path: `${prefix}XXXXXX`,
    message: `ENOENT: no such file or directory, mkstemp '${prefix}XXXXXX'`,
  };

  assert.throws(() => fs.mkstempSync(prefix), expected);
  fs.mkstemp(prefix, common.mustCall((err, file) => {
    assert.throws(() => { throw err; }, expected);
    assert.strictEqual(file, undefined);
  }));
  assert.rejects(fs.promises.mkstemp(prefix), expected).then(common.mustCall());
}

// Invalid arguments
{
  for (const prefix of [undefined, null, 1, {}, true]) {
    const expected = { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' };
    assert.throws(() => fs.mkstempSync(prefix), expected);
    assert.throws(() => fs.mkstemp(prefix, common.mustNotCall()), expected);
    assert.rejects(fs.promises.mkstemp(prefix), expected).then(common.mustCall());
  }

  assert.throws(() => fs.mkstemp(tmpdir.resolve('no-callback-')), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

  const withNull = { code: 'ERR_INVALID_ARG_VALUE', name: 'TypeError' };
  assert.throws(() => fs.mkstempSync('fhqwhgads\u0000'), withNull);
  assert.throws(() => fs.mkstemp('fhqwhgads\u0000', common.mustNotCall()), withNull);
  assert.rejects(fs.promises.mkstemp('fhqwhgads\u0000'), withNull).then(common.mustCall());
}
