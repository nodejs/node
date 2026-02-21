'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const file = tmpdir.resolve('write_stream_filehandle_test.txt');
const input = 'hello world';

tmpdir.refresh();

fs.promises.open(file, 'w+').then((handle) => {
  let calls = 0;
  const {
    write: originalWriteFunction,
    writev: originalWritevFunction
  } = handle;
  handle.write = function write() {
    calls++;
    return Reflect.apply(originalWriteFunction, this, arguments);
  };
  handle.writev = function writev() {
    calls++;
    return Reflect.apply(originalWritevFunction, this, arguments);
  };
  const stream = fs.createWriteStream(null, { fd: handle });

  stream.end(input);
  stream.on('close', common.mustCall(() => {
    assert(calls > 0, 'expected at least one call to fileHandle.write or ' +
    'fileHandle.writev, got 0');
  }));
}).then(common.mustCall());
