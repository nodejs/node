// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { it } = require('node:test');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');

tmpdir.refresh();
process.umask(0o000);

const isWindows = common.isWindows;
const files = [];
let count = 0;

function file() {
  const file = path.join(tmpdir.path,
                         `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
  files.push(file);
  return file;
}

it('mode sync', { skip: isWindows }, async (t) => {
  const dest = file();
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: true, mode });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      resolve();
    }));
  }));

  await promise;
});

it('mode', { skip: isWindows }, async (t) => {
  const dest = file();
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: false, mode });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      resolve();
    }));
  }));

  await promise;
});

it('mode default sync', { skip: isWindows }, async (t) => {
  const dest = file();
  const defaultMode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, defaultMode);
      resolve();
    }));
  }));

  await promise;
});

it('mode default', { skip: isWindows }, async (t) => {
  const dest = file();
  const defaultMode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, defaultMode);
      resolve();
    }));
  }));

  await promise;
});

it('mode on mkdir sync', { skip: isWindows }, async (t) => {
  const dest = path.join(file(), 'out.log');
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, mkdir: true, mode, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
      resolve();
    }));
  }));
  await promise;
});

it('mode on mkdir', { skip: isWindows }, async (t) => {
  const dest = path.join(file(), 'out.log');
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, mkdir: true, mode, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
      resolve();
    }));
  }));
  await promise;
});

it('mode on append sync', { skip: isWindows }, async (t) => {
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n', 'utf8', 0o422);
  const mode = isWindows ? 0o444 : 0o666;
  const stream = new FastUtf8Stream({ dest, append: false, mode, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'something else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('mode on append', { skip: isWindows }, async (t) => {
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n', 'utf8', 0o422);
  const mode = isWindows ? 0o444 : 0o666;
  const stream = new FastUtf8Stream({ dest, append: false, mode, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'something else\n');
      t.assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
      resolve();
    }));
  }));

  await promise;
});
