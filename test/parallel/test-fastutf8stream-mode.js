'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir'); ;
const {
  ok,
  strictEqual,
} = require('node:assert');
const {
  readFile,
  statSync,
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

const isWindows = process.platform === 'win32';


runTests(false);
runTests(true);

function runTests(sync) {
  {
    const dest = getTempFile();
    const mode = 0o666;
    const stream = new Utf8Stream({ dest, sync, mode });

    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
          // The actual mode may vary depending on the platform,
          // so we check only the first bit.
          strictEqual(statSync(dest).mode & 0o700, 0o600);
        }));
      }));
    }));
  }

  {
    const dest = getTempFile();
    const defaultMode = 0o600;
    const stream = new Utf8Stream({ dest, sync });
    stream.on('ready', common.mustCall(() => {
      ok(stream.write('hello world\n'));
      ok(stream.write('something else\n'));

      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'hello world\nsomething else\n');
          strictEqual(statSync(dest).mode & 0o700, defaultMode);
        }));
      }));
    }));
  }

  {
    const dest = join(getTempFile(), 'out.log');
    const mode = 0o666;
    const stream = new Utf8Stream({ dest, mkdir: true, mode, sync });

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
    // Create file with writable mode first, then change mode after Utf8Stream creation
    writeFileSync(dest, 'hello world\n', { encoding: 'utf8' });
    const mode = isWindows ? 0o444 : 0o666;
    const stream = new Utf8Stream({ dest, append: false, mode, sync });
    stream.on('ready', common.mustCall(() => {
      ok(stream.write('something else\n'));
      stream.flush();
      stream.on('drain', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'something else\n');
          stream.end();
        }));
      }));
    }));
  }
}
