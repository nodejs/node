'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const file = tmpdir.resolve('read_stream_filehandle_test.txt');
const input = 'hello world';

tmpdir.refresh();
fs.writeFileSync(file, input);

fs.promises.open(file, 'r').then((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });

  let output = '';
  stream.on('data', common.mustCallAtLeast((data) => {
    output += data;
  }));

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(output, input);
  }));

  stream.on('close', common.mustCall());
}).then(common.mustCall());

fs.promises.open(file, 'r').then((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('data', common.mustNotCall());
  stream.on('close', common.mustCall());

  return handle.close();
}).then(common.mustCall());

fs.promises.open(file, 'r').then((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('close', common.mustCall());

  stream.on('data', common.mustCall(() => {
    handle.close();
  }));
}).then(common.mustCall());

fs.promises.open(file, 'r').then((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createReadStream(null, { fd: handle });
  stream.on('close', common.mustCall());

  stream.close();
}).then(common.mustCall());

fs.promises.open(file, 'r').then((handle) => {
  assert.throws(() => {
    fs.createReadStream(null, { fd: handle, fs });
  }, {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    name: 'Error',
    message: 'The FileHandle with fs method is not implemented'
  });
  return handle.close();
}).then(common.mustCall());

fs.promises.open(file, 'r').then((handle) => {
  const { read: originalReadFunction } = handle;
  handle.read = common.mustCallAtLeast(function read() {
    return Reflect.apply(originalReadFunction, this, arguments);
  });

  const stream = fs.createReadStream(null, { fd: handle });

  let output = '';
  stream.on('data', common.mustCallAtLeast((data) => {
    output += data;
  }));

  stream.on('end', common.mustCall(() => {
    assert.strictEqual(output, input);
  }));
}).then(common.mustCall());
