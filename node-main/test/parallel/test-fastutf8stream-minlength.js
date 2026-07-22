'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const {
  ok,
  throws,
} = require('node:assert');
const {
  closeSync,
  openSync,
} = require('node:fs');
const { Utf8Stream } = require('node:fs');
const { join } = require('node:path');

tmpdir.refresh();
let fileCounter = 0;

function getTempFile() {
  return join(tmpdir.path, `fastutf8stream-${process.pid}-${Date.now()}-${fileCounter++}.log`);
}

const MAX_WRITE = 16 * 1024;

{
  const dest = getTempFile();
  const stream = new Utf8Stream({ dest, sync: false, minLength: 9999 });

  ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  ok(stream.write(Buffer.alloc(1500).fill('x').toString()));
  ok(!stream.write(Buffer.alloc(MAX_WRITE).fill('x').toString()));

  stream.on('drain', common.mustCall(() => stream.end()));

}

{
  const dest = getTempFile();
  const fd = openSync(dest, 'w');

  throws(() => {
    new Utf8Stream({
      fd,
      minLength: MAX_WRITE
    });
  }, Error);

  closeSync(fd);
}
