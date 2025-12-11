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
  writeFileSync,
} = require('node:fs');
const { join } = require('node:path');
const { Utf8Stream } = require('node:fs');
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
    writeFileSync(dest, 'hello world\n');
    const stream = new Utf8Stream({ dest, append: false, sync });

    stream.on('ready', () => {
      ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'something else\n');
          stream.end();
        }));
      }));
    });
  }

  {
    const dest = join(getTempFile(), 'out.log');
    const stream = new Utf8Stream({ dest, mkdir: true, sync });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\n');
          stream.end();
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));
      stream.flush();

      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
          stream.end();
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      stream.flush();
      stream.on('drain', common.mustCall(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.flush(common.mustSucceed(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });
    stream.on('ready', common.mustCall(() => {
      stream.flush(common.mustSucceed(() => {
        stream.end();
      }));
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 0, sync });

    stream.flush(common.mustSucceed(() => {
      stream.end();
    }));
  }

  {
    const dest = getTempFile();
    const fd = openSync(dest, 'w');
    const stream = new Utf8Stream({ fd, minLength: 4096, sync });
    stream.destroy();
    stream.flush(common.mustCall(ok));
  }
}
