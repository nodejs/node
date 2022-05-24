// Flags: --expose-internals
'use strict';
const common = require('../common');

const StreamWrap = require('internal/js_stream_socket');
const Duplex = require('stream').Duplex;

{
  const stream = new Duplex({
    read() {},
    write() {}
  });

  stream.setEncoding('ascii');

  const wrap = new StreamWrap(stream);

  wrap.on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_WRAP',
  }));

  stream.push('ohai');
}

{
  const stream = new Duplex({
    read() {},
    write() {},
    objectMode: true
  });

  const wrap = new StreamWrap(stream);

  wrap.on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_WRAP',
  }));

  stream.push(new Error('foo'));
}
