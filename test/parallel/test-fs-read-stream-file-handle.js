'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, 'read_stream_filehandle_test.txt');
const input = 'hello world';

let output = '';
tmpdir.refresh();
fs.writeFileSync(file, input);

fs.promises.open(file, 'r').then(common.mustCall((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });

  stream.on('data', common.mustCallAtLeast((data) => {
    output += data;
  }));

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(output, input);
  }));

  stream.on('close', common.mustCall());
}));

fs.promises.open(file, 'r').then(common.mustCall((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('data', common.mustNotCall());
  stream.on('close', common.mustCall());

  handle.close();
}));

fs.promises.open(file, 'r').then(common.mustCall((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('close', common.mustCall());

  stream.on('data', common.mustCall(() => {
    handle.close();
  }));
}));

fs.promises.open(file, 'r').then(common.mustCall((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('close', common.mustCall());

  stream.close();
}));

fs.promises.open(file, 'r').then(common.mustCall((handle) => {
  assert.throws(() => {
    fs.createReadStream(null, { fd: handle, fs });
  }, {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    name: 'Error',
    message: 'The FileHandle with fs method is not implemented'
  });
  handle.close();
}));
