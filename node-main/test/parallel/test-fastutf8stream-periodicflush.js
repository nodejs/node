'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  strictEqual,
} = require('node:assert');
const {
  openSync,
  readFile,
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
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, sync, minLength: 5000 });

    ok(stream.write('hello world\n'));

    setTimeout(common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        strictEqual(data, '');
      }));
    }), 1500);

    stream.destroy();
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');

    // Test that periodicFlush property is set correctly
    const stream1 = new Utf8Stream({ fd, sync, minLength: 5000 });
    strictEqual(stream1.periodicFlush, 0);
    stream1.destroy();

    const fd2 = openSync(dest, 'w');
    const stream2 = new Utf8Stream({ fd: fd2, sync, minLength: 5000, periodicFlush: 1000 });
    strictEqual(stream2.periodicFlush, 1000);
    stream2.destroy();
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, sync, minLength: 5000 });

    ok(stream.write('hello world\n'));

    // Manually flush to test that data can be written
    stream.flush(common.mustSucceed());

    setTimeout(common.mustCall(() => {
      readFile(dest, 'utf8', common.mustSucceed((data) => {
        strictEqual(data, 'hello world\n');
      }));
    }), 500);

    stream.destroy();
  }
}
