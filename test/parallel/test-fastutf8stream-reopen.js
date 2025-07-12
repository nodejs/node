// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');
const { it } = require('node:test');

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

it('reopen sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  const { promise, resolve } = Promise.withResolvers();
  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', () => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      });
    }));
  }));

  await promise;
});

it('reopen', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  const { promise, resolve } = Promise.withResolvers();
  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', () => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      });
    }));
  }));

  await promise;
});

it('reopen if not open sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.reopen();

  stream.end();
  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));
  await promise;
});

it('reopen if not open', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.reopen();

  stream.end();
  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));
  await promise;
});

it('reopen with file sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 0, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-new';

  const { promise, resolve } = Promise.withResolvers();
  stream.once('drain', common.mustCall(() => {
    stream.reopen(after);
    t.assert.strictEqual(stream.file, after);

    stream.once('ready', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      }));
    }));
  }));

  await promise;
});

it('reopen with file', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 0, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-new';

  const { promise, resolve } = Promise.withResolvers();
  stream.once('drain', common.mustCall(() => {
    stream.reopen(after);
    t.assert.strictEqual(stream.file, after);

    stream.once('ready', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      }));
    }));
  }));

  await promise;
});

it('reopen emits drain sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  const { promise, resolve } = Promise.withResolvers();

  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('drain', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      }));
    }));
  }));

  await promise;
});

it('reopen emits drain', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  const { promise, resolve } = Promise.withResolvers();

  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('drain', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          t.assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            t.assert.strictEqual(data, 'after reopen\n');
            stream.end();
            resolve();
          }));
        }));
      }));
    }));
  }));

  await promise;
});
