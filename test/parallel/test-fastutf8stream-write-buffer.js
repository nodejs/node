'use strict';

// Regression test for buffer content mode: the first buffer of every
// batch used to be dropped, corrupting the output and eventually
// crashing in mergeBuf(). Refs: https://github.com/nodejs/node/pull/58897

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const {
  readFile,
  Utf8Stream,
} = require('node:fs');
const { join } = require('node:path');

tmpdir.refresh();
let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

runTests(false);
runTests(true);

function runTests(sync) {
  {
    // A single buffer write must end up in the file.
    const dest = getTempFile();
    const stream = new Utf8Stream({ dest, sync, contentMode: 'buffer' });

    stream.on('ready', common.mustCall(() => {
      assert.ok(stream.write(Buffer.from('hello world\n')));
      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(data, 'hello world\n');
        }));
      }));
    }));
  }

  {
    // Writes that exceed maxWrite start a new batch; data must survive
    // the batch boundary and be written in order.
    const dest = getTempFile();
    const stream = new Utf8Stream({
      dest,
      sync,
      contentMode: 'buffer',
      minLength: 60,
      maxWrite: 64,
    });

    stream.on('ready', common.mustCall(() => {
      stream.write(Buffer.from('a'.repeat(40)));
      stream.write(Buffer.from('b'.repeat(40)));
      stream.write(Buffer.from('c'.repeat(40)));
      stream.end();

      stream.on('finish', common.mustCall(() => {
        readFile(dest, 'utf8', common.mustSucceed((data) => {
          assert.strictEqual(
            data,
            'a'.repeat(40) + 'b'.repeat(40) + 'c'.repeat(40),
          );
        }));
      }));
    }));
  }
}
