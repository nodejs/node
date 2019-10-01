'use strict';
const common = require('../common');
const assert = require('assert');
const stream = require('stream');

{
  const ws = new stream.Writable();
  const rs = new stream.Readable({ highWaterMark: 16 });

  ws._write = common.mustNotCall();

  rs._read = function() {
    rs.push('hello world');
  };

  rs.pipe(ws);
  ws.destroy();
  ws.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
  }));
}

{
  const ws = new stream.Writable();
  const rs = new stream.Readable({ highWaterMark: 16 });

  ws.write = common.mustNotCall();

  rs._read = function() {
    rs.push('hello world');
  };

  rs.pipe(ws);
  ws.destroy(new Error('asd'));
  ws.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'asd');
  }));
}
