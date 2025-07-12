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

const files = [];
let count = 0;

function file() {
  const file = path.join(tmpdir.path,
                         `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
  files.push(file);
  return file;
}

it('append sync', async (t) => {
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n');
  const stream = new FastUtf8Stream({ dest, append: false, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'something else\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('append', async (t) => {
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n');
  const stream = new FastUtf8Stream({ dest, append: false, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'something else\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('mkdir sync', async (t) => {
  const dest = path.join(file(), 'out.log');
  const stream = new FastUtf8Stream({ dest, mkdir: true, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('mkdir', async (t) => {
  const dest = path.join(file(), 'out.log');
  const stream = new FastUtf8Stream({ dest, mkdir: true, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('flush sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('flush', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flush();

  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      stream.end();
      resolve();
    }));
  }));

  await promise;
});

it('flush with no data sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  stream.flush(common.mustCall());

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(resolve));
  await promise;
});

it('flush with no data', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  stream.flush(common.mustCall());

  const { promise, resolve } = Promise.withResolvers();

  stream.on('drain', common.mustCall(resolve));
  await promise;
});

it('call flush cb after flushed sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const { promise, resolve } = Promise.withResolvers();

  stream.flush(common.mustCall(resolve));

  await promise;
});

it('call flush cb after flushed', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const { promise, resolve } = Promise.withResolvers();

  stream.flush(common.mustCall(resolve));

  await promise;
});

it('call flush cb even when have no data sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  const { promise, resolve } = Promise.withResolvers();
  stream.on('ready', common.mustCall(() => {
    stream.flush(common.mustCall(resolve));
  }));

  await promise;
});

it('call flush cb even when have no data', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  const { promise, resolve } = Promise.withResolvers();
  stream.on('ready', common.mustCall(() => {
    stream.flush(common.mustCall(resolve));
  }));

  await promise;
});

it('call flush cb even when minLength is 0 sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall(resolve));
  await promise;
});

it('call flush cb even when minLength is 0', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall(resolve));
  await promise;
});

it('call flush cb with an error when trying to flush destroyed stream sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });
  stream.destroy();

  const { promise, resolve, reject } = Promise.withResolvers();
  stream.flush(common.mustCall((err) => {
    if (err) resolve();
    else reject(new Error('flush cb called without an error'));
  }));

  await promise;
});

it('call flush cb with an error when trying to flush destroyed stream', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });
  stream.destroy();

  const { promise, resolve, reject } = Promise.withResolvers();
  stream.flush(common.mustCall((err) => {
    if (err) resolve();
    else reject(new Error('flush cb called without an error'));
  }));

  await promise;
});
