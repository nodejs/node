'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
} = require('node:assert');
const {
  openSync,
  fsyncSync,
  writeSync,
  write,
} = require('node:fs');
const { join } = require('node:path');
const { Utf8Stream } = require('node:fs');
const { isMainThread } = require('node:worker_threads');

tmpdir.refresh();
if (isMainThread) {
  process.umask(0o000);
}

let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

function runTests(sync) {

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const fsOverride = {
      fsync: common.mustNotCall(),
      fsyncSync: common.mustCall(() => fsyncSync(fd)),
    };
    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => writeSync(...args));
      fsOverride.write = common.mustNotCall();
    } else {
      fsOverride.write = common.mustCall((...args) => write(...args));
      fsOverride.writeSync = common.mustNotCall();
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      fsync: true,
      minLength: 4096,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));

      stream.flush(common.mustSucceed(() => stream.end()));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    const testError = new Error('fsync failed');
    testError.code = 'ETEST';

    const fsOverride = {
      fsync: common.mustCall((fd, cb) => {
        process.nextTick(() => cb(testError));
      }, 2),
    };

    if (sync) {
      fsOverride.writeSync = common.mustCall((...args) => {
        return writeSync(...args);
      });
    } else {
      fsOverride.write = common.mustCall((...args) => {
        return write(...args);
      });
    }

    const stream = new Utf8Stream({
      fd,
      sync,
      minLength: 4096,
      fs: fsOverride,
    });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      stream.flush(common.mustCall((err) => {
        ok(err, 'flush should return an error');
        strictEqual(err.code, 'ETEST');
        stream.end();
      }));
    }));
  }
}
