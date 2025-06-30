'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
  throws,
} = require('node:assert');
const {
  open,
  openSync,
  readFile,
  renameSync,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { join } = require('node:path');
const { isMainThread } = require('node:worker_threads');

tmpdir.refresh();
let fileCounter = 0;

if (isMainThread) {
  process.umask(0o000);
}

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

function runTests(sync) {

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    const after = dest + '-moved';

    stream.once('drain', common.mustCall(() => {
      renameSync(dest, after);
      stream.reopen();

      stream.once('ready', common.mustCall(() => {
        ok(stream.write('after reopen\n'));

        stream.once('drain', common.mustCall(() => {
          readFile(after, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\n');
            readFile(dest, 'utf8', common.mustSucceed((data) => {
              strictEqual(data, 'after reopen\n');
              stream.end();
            }));
          }));
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    stream.reopen();
    stream.end();

    stream.on('close', common.mustCall());
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, minLength: 0, sync });

    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    const after = dest + '-new';

    stream.once('drain', common.mustCall(() => {
      stream.reopen(after);
      strictEqual(stream.file, after);

      stream.once('ready', common.mustCall(() => {
        ok(stream.write('after reopen\n'));

        stream.once('drain', common.mustCall(() => {
          readFile(dest, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\n');
            readFile(after, 'utf8', common.mustSucceed((data) => {
              strictEqual(data, 'after reopen\n');
              stream.end();
            }));
          }));
        }));
      }));
    }));
  }

  {
    let throwOnNextOpen = false;
    const err = new Error('open error');
    const fsOverride = {};
    if (sync) {
      fsOverride.openSync = function(...args) {
        if (throwOnNextOpen) {
          throwOnNextOpen = false;
          throw err;
        }
        return openSync(...args);
      };
    } else {
      fsOverride.open = function(file, flags, mode, cb) {
        if (throwOnNextOpen) {
          throwOnNextOpen = false;
          process.nextTick(() => cb(err));
          return;
        }
        return open(file, flags, mode, cb);
      };
    }

    const dest = getTempFile();
    const stream = new Utf8Stream({
      dest,
      sync,
      fs: fsOverride,
    });

    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    const after = dest + '-moved';
    stream.on('error', common.mustCall());

    stream.once('drain', common.mustCall(() => {
      renameSync(dest, after);
      throwOnNextOpen = true;
      if (sync) {
        throws(() => stream.reopen(), err);
      } else {
        stream.reopen();
      }

      setTimeout(common.mustCall(() => {
        ok(stream.write('after reopen\n'));

        stream.end();
        stream.on('finish', common.mustCall(() => {
          readFile(after, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\nafter reopen\n');
          }));
        }));
      }), 10);
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });

    ok(stream.write('hello world\n'));
    ok(stream.write('something else\n'));

    const after = dest + '-moved';
    stream.once('drain', common.mustCall(() => {
      renameSync(dest, after);
      stream.reopen();

      stream.once('drain', common.mustCall(() => {
        ok(stream.write('after reopen\n'));

        stream.once('drain', common.mustCall(() => {
          readFile(after, 'utf8', common.mustSucceed((data) => {
            strictEqual(data, 'hello world\nsomething else\n');
            readFile(dest, 'utf8', common.mustSucceed((data) => {
              strictEqual(data, 'after reopen\n');
              stream.end();
            }));
          }));
        }));
      }));
    }));
  }
}
