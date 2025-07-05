// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');
const FastUtf8Stream = require('internal/streams/fast-utf8-stream');
const { isMainThread } = require('worker_threads');
const assert = require('assert');

if (isMainThread) {
  process.umask(0o000);
}

tmpdir.refresh();

const isWindows = common.isWindows;
const kMaxWrite = 16 * 1024;
let count = 0;

function file() {
  return path.join(tmpdir.path,
                   `sonic-boom-${process.pid}-${process.hrtime().toString()}-${count++}`);
}

{
  // Destroy Sync
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  assert.ok(stream.write('hello world\n'));
  stream.destroy();
  assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  const data = fs.readFileSync(dest, 'utf8');
  assert.strictEqual(data, 'hello world\n');

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  assert.ok(stream.write('hello world\n'));
  stream.destroy();
  assert.throws(() => { stream.write('hello world\n'); }, {
    code: 'ERR_INVALID_STATE',
  });

  stream.on('finish', common.mustNotCall());
  stream.on('close', common.mustCall(() => {
    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\n');
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest });

  stream.destroy();

  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));

}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    const after = dest + '-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));

}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });

  stream.once('ready', common.mustCall(() => {
    stream.reopen(dest + '-moved');
    const after = dest + '-moved-moved';
    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, 'after reopen\n');
      }));
    }));
    stream.end();
  }));

}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: false });
  const after = dest + '-moved';
  stream.reopen(after);
  stream.write('after reopen\n');

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'after reopen\n');
    }));
  }));
  stream.end();
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flushSync();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall());
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flushSync();

  // Let the file system settle down things
  setImmediate(common.mustCall(() => {
    stream.end();
    const data = fs.readFileSync(dest, 'utf8');
    assert.strictEqual(data, 'hello world\nsomething else\n');
    stream.on('close', common.mustCall());
  }));
}

{
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n');
  const stream = new FastUtf8Stream({ dest, append: false, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('something else\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'something else\n');
      stream.end();
    }));
  }));

}

{
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n');
  const stream = new FastUtf8Stream({ dest, append: false, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('something else\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'something else\n');
      stream.end();
    }));
  }));
}

{
  const dest = path.join(file(), 'out.log');
  const stream = new FastUtf8Stream({ dest, mkdir: true, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\n');
      stream.end();
    }));
  }));
}

{
  const dest = path.join(file(), 'out.log');
  const stream = new FastUtf8Stream({ dest, mkdir: true, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\n');
      stream.end();
    }));
  }));

}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      stream.end();
    }));
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flush();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      stream.end();
    }));
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  stream.flush(common.mustCall());

  stream.on('drain', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  stream.flush(common.mustCall());

  stream.on('drain', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flush(common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.flush(common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });

  stream.on('ready', common.mustCall(() => {
    stream.flush(common.mustCall());
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });
  stream.on('ready', common.mustCall(() => {
    stream.flush(common.mustCall());
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  stream.flush(common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: false });

  stream.flush(common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: true });
  stream.destroy();

  stream.flush(common.mustCall((err) => {
    assert.ok(err);
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 4096, sync: false });
  stream.destroy();
  stream.flush(common.mustCall((err) => {
    assert.ok(err);
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false, minLength: 9999 });

  assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  assert.ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  assert.ok(!stream.write(Buffer.alloc(kMaxWrite).fill('x').toString()));
  stream.on('drain', common.mustCall());
}

{
  assert.throws(() => {
    const dest = file();
    const fd = fs.openSync(dest, 'w');

    new FastUtf8Stream({
      fd,
      minLength: kMaxWrite
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  const dest = file();
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: true, mode });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
    }));
  }));
}

{
  const dest = file();
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: false, mode });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();
  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
    }));
  }));
}

{
  const dest = file();
  const defaultMode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, defaultMode);
    }));
  }));
}

{
  const dest = file();
  const defaultMode = 0o666;
  const stream = new FastUtf8Stream({ dest, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, defaultMode);
    }));
  }));
}

{
  const dest = path.join(file(), 'out.log');
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, mkdir: true, mode, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));

  stream.flush();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
    }));
  }));
}

{
  const dest = path.join(file(), 'out.log');
  const mode = 0o666;
  const stream = new FastUtf8Stream({ dest, mkdir: true, mode, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));

  stream.flush();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
    }));
  }));
}

{
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n', 'utf8', 0o422);
  const mode = isWindows ? 0o444 : 0o666;
  const stream = new FastUtf8Stream({ dest, append: false, mode, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('something else\n'));

  stream.flush();
  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'something else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
    }));
  }));
}

{
  const dest = file();
  fs.writeFileSync(dest, 'hello world\n', 'utf8', 0o422);
  const mode = isWindows ? 0o444 : 0o666;
  const stream = new FastUtf8Stream({ dest, append: false, mode, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('something else\n'));

  stream.flush();

  stream.on('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'something else\n');
      assert.strictEqual(fs.statSync(dest).mode & 0o777, stream.mode);
      stream.end();
    }));
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      assert.ok(stream.write('after reopen\n'));

      stream.once('drain', () => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            assert.strictEqual(data, 'after reopen\n');
            stream.end();
          }));
        }));
      });
    }));
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: false });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';
  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      assert.ok(stream.write('after reopen\n'));

      stream.once('drain', () => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            assert.strictEqual(data, 'after reopen\n');
            stream.end();
          }));
        }));
      });
    }));
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 4096, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  stream.once('ready', common.mustCall(() => {
    stream.flush();
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('ready', common.mustCall(() => {
      assert.ok(stream.write('after reopen\n'));
      stream.flush();

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            assert.strictEqual(data, 'after reopen\n');
            stream.end();
          }));
        }));
      }));
    }));
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.reopen();

  stream.end();
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, minLength: 0, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  const after = dest + '-new';
  stream.once('drain', common.mustCall(() => {
    stream.reopen(after);
    assert.strictEqual(stream.file, after);

    stream.once('ready', common.mustCall(() => {
      assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
            assert.strictEqual(data, 'after reopen\n');
            stream.end();
          }));
        }));
      }));
    }));
  }));
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  const after = dest + '-moved';

  stream.once('drain', common.mustCall(() => {
    fs.renameSync(dest, after);
    stream.reopen();

    stream.once('drain', common.mustCall(() => {
      assert.ok(stream.write('after reopen\n'));

      stream.once('drain', common.mustCall(() => {
        fs.promises.readFile(after, 'utf8').then(common.mustCall((data) => {
          assert.strictEqual(data, 'hello world\nsomething else\n');
          fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
            assert.strictEqual(data, 'after reopen\n');
            stream.end();
          }));
        }));
      }));
    }));
  }));
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, minLength: 0, sync: true });

  let buf = Buffer.alloc((1024 * 16) - 2).fill('x'); // 16MB - 3B
  const length = buf.length + 4;
  buf = buf.toString() + '🌲'; // 16 MB + 1B

  stream.write(buf);

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.stat(dest, common.mustSucceed((stat) => {
      assert.strictEqual(stat.size, length);
      const char = Buffer.alloc(4);
      const fd = fs.openSync(dest, 'r');
      fs.readSync(fd, char, 0, 4, length - 4);
      assert.strictEqual(char.toString(), '🌲');
    }));
  }));

  stream.on('close', common.mustCall());
}

// For context see this issue https://github.com/pinojs/pino/issues/871
{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });
  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));
  stream.flushSync();
}

{
  assert.throws(() => new FastUtf8Stream({ dest: '/path/to/nowwhere', sync: true }), {
    code: 'ENOENT',
  });
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: false });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });

  stream.once('drain', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\n');
      assert.ok(stream.write('something else\n'));
    }));

    stream.once('drain', common.mustCall(() => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, 'hello world\nsomething else\n');
        stream.end();
      }));
    }));
  }));

  assert.ok(stream.write('hello world\n'));

  stream.on('finish', common.mustCall());
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const fd = fs.openSync(dest, 'w');
  const stream = new FastUtf8Stream({ fd, sync: true });
  const source = fs.createReadStream(__filename, { encoding: 'utf8' });

  source.pipe(stream);

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(__filename, 'utf8').then(common.mustCall((expected) => {
      fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
        assert.strictEqual(data, expected);
      }));
    }));
  }));

  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
    }));
  }));
  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, sync: true });

  stream.on('ready', common.mustCall());

  let length = 0;
  stream.on('write', common.mustCall((bytes) => {
    length += bytes;
  }, 2));

  assert.ok(stream.write('hello world\n'));
  assert.ok(stream.write('something else\n'));

  stream.end();

  stream.on('finish', common.mustCall(() => {
    fs.promises.readFile(dest, 'utf8').then(common.mustCall((data) => {
      assert.strictEqual(data, 'hello world\nsomething else\n');
      assert.strictEqual(length, 27);
    }));
  }));
  stream.on('close', common.mustCall());
}

{
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
    fs.stat(dest, common.mustSucceed((stat) => {
      assert.strictEqual(stat.size, length);
    }));
  }));

  stream.on('close', common.mustCall());
}

{
  const dest = file();
  const stream = new FastUtf8Stream({ dest, maxLength: 65536 });
  assert.strictEqual(stream.maxLength, 65536);
}
