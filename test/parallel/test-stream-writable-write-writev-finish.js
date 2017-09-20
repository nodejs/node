'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

// ensure consistency between the finish event when using cork()
// and writev and when not using them

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    cb(new Error('write test error'));
  };

  let firstError = false;
  writable.on('finish', common.mustCall(function() {
    assert.strictEqual(firstError, true);
  }));

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'write test error');
    firstError = true;
  }));

  writable.end('test');
}

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    setImmediate(cb, new Error('write test error'));
  };

  let firstError = false;
  writable.on('finish', common.mustCall(function() {
    assert.strictEqual(firstError, true);
  }));

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'write test error');
    firstError = true;
  }));

  writable.end('test');
}

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    cb(new Error('write test error'));
  };

  writable._writev = (chunks, cb) => {
    cb(new Error('writev test error'));
  };

  let firstError = false;
  writable.on('finish', common.mustCall(function() {
    assert.strictEqual(firstError, true);
  }));

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'writev test error');
    firstError = true;
  }));

  writable.cork();
  writable.write('test');

  setImmediate(function() {
    writable.end('test');
  });
}

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    setImmediate(cb, new Error('write test error'));
  };

  writable._writev = (chunks, cb) => {
    setImmediate(cb, new Error('writev test error'));
  };

  let firstError = false;
  writable.on('finish', common.mustCall(function() {
    assert.strictEqual(firstError, true);
  }));

  writable.on('prefinish', common.mustCall());

  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'writev test error');
    firstError = true;
  }));

  writable.cork();
  writable.write('test');

  setImmediate(function() {
    writable.end('test');
  });
}

// Regression test for
// https://github.com/nodejs/node/issues/13812

{
  const rs = new stream.Readable();
  rs.push('ok');
  rs.push(null);
  rs._read = () => {};

  const ws = new stream.Writable();
  let firstError = false;

  ws.on('finish', common.mustCall(function() {
    assert.strictEqual(firstError, true);
  }));
  ws.on('error', common.mustCall(function() {
    firstError = true;
  }));

  ws._write = (chunk, encoding, done) => {
    setImmediate(done, new Error());
  };
  rs.pipe(ws);
}

{
  const rs = new stream.Readable();
  rs.push('ok');
  rs.push(null);
  rs._read = () => {};

  const ws = new stream.Writable();

  ws.on('finish', common.mustNotCall());
  ws.on('error', common.mustCall());

  ws._write = (chunk, encoding, done) => {
    done(new Error());
  };
  rs.pipe(ws);
}
