'use strict';

const common = require('../common');
const assert = require('assert');
const stream = require('stream');

// Ensure consistency between the finish event when using cork()
// and writev and when not using them

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    cb(new Error('write test error'));
  };

  writable.on('finish', common.mustNotCall());
  writable.on('prefinish', common.mustNotCall());
  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'write test error');
  }));

  writable.end('test');
}

{
  const writable = new stream.Writable();

  writable._write = (chunks, encoding, cb) => {
    setImmediate(cb, new Error('write test error'));
  };

  writable.on('finish', common.mustNotCall());
  writable.on('prefinish', common.mustNotCall());
  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'write test error');
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

  writable.on('finish', common.mustNotCall());
  writable.on('prefinish', common.mustNotCall());
  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'writev test error');
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

  writable.on('finish', common.mustNotCall());
  writable.on('prefinish', common.mustNotCall());
  writable.on('error', common.mustCall((er) => {
    assert.strictEqual(er.message, 'writev test error');
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

  ws.on('finish', common.mustNotCall());
  ws.on('error', common.mustCall());

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

{
  const w = new stream.Writable();
  w._write = (chunk, encoding, cb) => {
    process.nextTick(cb);
  };
  w.on('error', common.mustCall());
  w.on('finish', common.mustNotCall());
  w.on('prefinish', () => {
    w.write("shouldn't write in prefinish listener");
  });
  w.end();
}

{
  const w = new stream.Writable();
  w._write = (chunk, encoding, cb) => {
    process.nextTick(cb);
  };
  w.on('error', common.mustCall());
  w.on('finish', () => {
    w.write("shouldn't write in finish listener");
  });
  w.end();
}
