// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');
const { it } = require('node:test');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('Setting process.umask is not supported in Workers');
}

tmpdir.refresh();
process.umask(0o000);

const isWindows = common.isWindows;
const kMaxWrite = 16 * 1024;
const files = [];
let count = 0;

function file() {
  const file = path.join(tmpdir.path,
                         `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
  files.push(file);
  return file;
}

it('destroy sync', async (t) => {
  t.plan(3);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  stream.destroy();
  t.assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  const data = await fs.promises.readFile(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\n');

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
});

it('destroy', async (t) => {
  t.plan(3);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  stream.destroy();
  t.assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  const data = await fs.promises.readFile(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\n');

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
});

it('destroy while opening', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest });

  stream.destroy();
  stream.on('close', common.mustCall());
});

it('end after reopen sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after reopen', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after 2x reopen sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end after 2x reopen', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
});

it('end if not ready sync', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
});

it('end if not ready', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
});

it('flushSync sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flushSync();

  const { promise, resolve } = Promise.withResolvers();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    t.assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall(resolve));
  }));
  await promise;
});

it('flushSync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.flushSync();

  const { promise, resolve } = Promise.withResolvers();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    t.assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall(resolve));
  }));
  await promise;
});

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

it('drain deadlock', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false, minLength: 9999 });

  t.assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  t.assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  t.assert.ok(!stream.write(Buffer.alloc(kMaxWrite).fill('x').toString()));
  const { promise, resolve } = Promise.withResolvers();
  stream.on('drain', common.mustCall(resolve));
  await promise;
});

it('should throw if minLength >= maxWrite', (t) => {
  t.assert.throws(() => {
    const dest = file();
    const fd = fs.openSync(dest, 'w');

    new FastUtf8Stream({
      fd,
      minLength: kMaxWrite
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
});

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

it('reopen with buffer sync', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  const { promise, resolve } = Promise.withResolvers();
  stream.once('ready', common.mustCall(() => {
    stream.flush();
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      t.assert.ok(stream.write('after reopen\n'));
      stream.flush();

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

it('reopen if not open', async (t) => {
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

it('reopen with file', async (t) => {
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

it('reopen emits drain', async (t) => {
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

it('write enormously large buffers sync with utf8 multi-byte split', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  let buf = Buffer.alloc((1024 * 16) - 2).fill('x'); // 16MB - 3B
  const length = buf.length + 4;
  buf = buf.toString() + '🌲'; // 16 MB + 1B

  stream.write(buf);

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustCall((err, stat) => {
      t.assert.ifError(err);
      t.assert.strictEqual(stat.size, length);
      const char = Buffer.alloc(4);
      const fd = fs.openSync(dest, 'r');
      fs.readSync(fd, char, 0, 4, length - 4);
      t.assert.strictEqual(char.toString(), '🌲');
    }));
  }));

  stream.on('close', common.mustCall(resolve));

  await promise;
});

// For context see this issue https://github.com/pinojs/pino/issues/871
it('file specified by dest path available immediately when options.sync is true', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });
  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));
  stream.flushSync();
});

it('sync error handling', (t) => {
  t.assert.throws(() => new FastUtf8Stream({ dest: '/path/to/nowwhere', sync: true }), {
    code: 'ENOENT',
  });
});

it('write things to a file descriptor sync', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      resolve();
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things to a file descriptor', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      resolve();
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things in a streaming fashion', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.once('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\n');
      t.assert.ok(stream.write('something else\n'));
    }));

    stream.once('drain', common.mustCall(() => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, 'hello world\nsomething else\n');
        stream.end();
      }));
    }));
  }));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall());
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('can be piped into', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });
  const source = fs.createReadStream(__filename, { encoding: 'utf8' });

  source.pipe(stream);

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(__filename, 'utf8').then(common.mustCall((expected) => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        t.assert.strictEqual(data, expected);
      }));
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write things to a file', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('emit write events', async (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  let length = 0;
  stream.on('write', common.mustCall((bytes) => {
    length += bytes;
  }, 2));

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const { promise, resolve } = Promise.withResolvers();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
      t.assert.strictEqual(length, 27);
    }));
  }));
  stream.on('close', common.mustCall(resolve));

  await promise;
});

it('write enormously large buffers async', async (t) => {
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  const buf = Buffer.alloc(1024).fill('x').toString(); // 1 MB
  let length = 0;

  for (let i = 0; i < 1024 * 512; i++) {
    length += buf.length;
    stream.write(buf);
  }

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustCall((err, stat) => {
      t.assert.ifError(err);
      t.assert.strictEqual(stat.size, length);
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();

  stream.on('close', common.mustCall(resolve));
  await promise;
});

it('make sure `maxWrite` is passed', (t) => {
  const dest = file();
  const stream = new FastUtf8Stream({ dest, maxLength: 65536 });
  t.assert.strictEqual(stream.maxLength, 65536);
});

it('only call fsyncSync and not fsync when fsync: true sync', async (t) => {

  const originalFsync = fs.fsync;
  const originalFsyncSync = fs.fsyncSync;
  const originalWriteSync = fs.writeSync;

  fs.fsync = common.mustNotCall();
  fs.fsyncSync = common.mustCall();

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    fsync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.writeSync = common.mustCall((...args) => originalWriteSync(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustSucceed(() => {
    process.nextTick(common.mustCall(resolve));
  }));

  await promise;

  fs.writeSync = originalWriteSync;
  fs.fsync = originalFsync;
  fs.fsyncSync = originalFsyncSync;
});

it('only call fsyncSync and not fsync when fsync: true', async (t) => {

  const originalFsync = fs.fsync;
  const originalFsyncSync = fs.fsyncSync;
  const originalWrite = fs.write;

  fs.fsync = common.mustNotCall();
  fs.fsyncSync = common.mustCall();

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: false,
    fsync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.write = common.mustCall((...args) => originalWrite(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustSucceed(() => {
    process.nextTick(common.mustCall(resolve));
  }));

  await promise;

  fs.write = originalWrite;
  fs.fsync = originalFsync;
  fs.fsyncSync = originalFsyncSync;
});

it('call flush cb with error when fsync failed sync', async (t) => {

  const originalFsync = fs.fsync;
  const originalWriteSync = fs.writeSync;

  const err = new Error('boom');
  fs.fsync = common.mustCall((...args) => {
    args[args.length - 1](err);
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  fs.writeSync = common.mustCall((...args) => originalWriteSync(...args));

  t.assert.ok(stream.write('hello world\n'));

  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall((actual) => {
    t.assert.strictEqual(actual, err);
    resolve();
  }));

  await promise;

  fs.writeSync = originalWriteSync;
  fs.fsync = originalFsync;
});


it('call flush cb with an error when failed to flush sync', async (t) => {

  const originalWriteSync = fs.writeSync;
  const err = new Error('boom');
  fs.writeSync = common.mustCall(() => {
    throw err;
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));
  const { promise, resolve } = Promise.withResolvers();
  stream.flush(common.mustCall((actual) => {
    t.assert.strictEqual(actual, err);
    fs.writeSync = originalWriteSync;
  }));

  stream.end();

  stream.on('close', common.mustCall(resolve));

  await promise;

});

it('call flush cb when finish writing when currently in the middle sync', async (t) => {

  const originalWriteSync = fs.writeSync;

  const { promise, resolve } = Promise.withResolvers();

  fs.writeSync = common.mustCall((...args) => {
    stream.flush(common.mustSucceed(resolve));
    originalWriteSync(...args);
  });

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({
    fd,
    sync: true,

    // To trigger write without calling flush
    minLength: 1
  });

  stream.on('ready', common.mustCall());

  t.assert.ok(stream.write('hello world\n'));

  await promise;

  fs.writeSync = originalWriteSync;
});

it('call flush cb when writing and trying to flush before ready (on async)', async (t) => {

  const originalOpen = fs.open;

  const { promise, resolve } = Promise.withResolvers();
  fs.open = function(...args) {
    process.nextTick(common.mustCall(() => {
      // Try writing and flushing before ready and in the middle of opening
      t.assert.ok(stream.write('hello world\n'));
      // calling flush
      stream.flush(common.mustSucceed(resolve));
      originalOpen(...args);
    }));
  };

  const dest = file();
  const stream = new FastUtf8Stream({
    fd: dest,
    // Only async as sync is part of the constructor so the user will not be able to call write/flush
    // before ready
    sync: false,

    // To not trigger write without calling flush
    minLength: 4096
  });

  stream.on('ready', common.mustCall());

  await promise;

  fs.open = originalOpen;
});

it('fsync with sync', async (t) => {

  const originalFsyncSync = fs.fsyncSync;
  fs.fsyncSync = common.mustCall(originalFsyncSync, 2);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true, fsync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  const data = fs.readFileSync(dest, 'utf8');
  t.assert.strictEqual(data, 'hello world\nsomething else\n');
  fs.fsyncSync = originalFsyncSync;
});

it('fsync with async', async (t) => {

  const originalFsyncSync = fs.fsyncSync;
  fs.fsyncSync = common.mustCall(originalFsyncSync, 2);

  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, fsync: true });

  t.assert.ok(stream.write('hello world\n'));
  t.assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.readFile(dest, 'utf8', common.mustSucceed((data) => {
      t.assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));

  const { promise, resolve } = Promise.withResolvers();
  stream.on('close', common.mustCall(resolve));

  await promise;

  fs.fsyncSync = originalFsyncSync;
});

