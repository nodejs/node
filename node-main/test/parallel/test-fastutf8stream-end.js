'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  strictEqual,
} = require('node:assert');
const fs = require('node:fs');
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
    const stream = new Utf8Stream({ dest, minLength: 4096, sync });

    stream.once('ready', common.mustCall(() => {
      const after = `${dest}-moved`;
      stream.reopen(after);
      stream.write('after reopen\n');
      stream.on('finish', common.mustCall(() => {
        fs.readFile(after, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'after reopen\n');
        }));
      }));
      stream.end();
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, minLength: 4096, sync });

    stream.once('ready', common.mustCall(() => {
      stream.reopen(`${dest}-moved`);
      const after = `${dest}-moved-moved`;
      stream.reopen(after);
      stream.write('after reopen\n');
      stream.on('finish', common.mustCall(() => {
        fs.readFile(after, 'utf8', common.mustSucceed((data) => {
          strictEqual(data, 'after reopen\n');
        }));
      }));
      stream.end();
    }));
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, minLength: 4096, sync });
    const after = dest + '-moved';

    stream.reopen(after);
    stream.write('after reopen\n');
    stream.on('finish', common.mustCall(() => {
      fs.readFile(after, 'utf8', common.mustSucceed((data) => {
        strictEqual(data, 'after reopen\n');
      }));
    }));

    stream.end();
  }

  {
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync });
    const str = Buffer.alloc(10000).fill('a').toString();

    let totalWritten = 0;
    function writeData() {
      if (totalWritten >= 10000) {
        stream.end();
        return;
      }

      const chunk = str.slice(totalWritten, totalWritten + 1000);
      if (stream.write(chunk)) {
        totalWritten += chunk.length;
        setImmediate(common.mustCall(writeData));
      } else {
        stream.once('drain', common.mustCall(() => {
          totalWritten += chunk.length;
          setImmediate(common.mustCall(writeData));
        }));
      }
    };

    stream.on('finish', common.mustCall(() => {
      fs.readFile(dest, 'utf8', common.mustSucceed((data) => {
        strictEqual(data.length, 10000);
        strictEqual(data, str);
      }));
    }));

    writeData();
  }
}
