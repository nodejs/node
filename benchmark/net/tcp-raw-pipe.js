// In this benchmark, we connect a client to the server, and write
// as many bytes as we can in the specified time (default = 10s)
'use strict';

const common = require('../common.js');
const util = require('util');

// if there are --dur=N and --len=N args, then
// run the function with those settings.
// if not, then queue up a bunch of child processes.
const bench = common.createBenchmark(main, {
  len: [102400, 1024 * 1024 * 16],
  type: ['utf', 'asc', 'buf'],
  dur: [5]
});

function fail(err, syscall) {
  throw util._errnoException(err, syscall);
}

const { TCP, constants: TCPConstants } = process.binding('tcp_wrap');
const TCPConnectWrap = process.binding('tcp_wrap').TCPConnectWrap;
const WriteWrap = process.binding('stream_wrap').WriteWrap;
const PORT = common.PORT;

function main({ dur, len, type }) {
  // Server
  const serverHandle = new TCP(TCPConstants.SERVER);
  var err = serverHandle.bind('127.0.0.1', PORT);
  if (err)
    fail(err, 'bind');

  err = serverHandle.listen(511);
  if (err)
    fail(err, 'listen');

  serverHandle.onconnection = function(err, clientHandle) {
    if (err)
      fail(err, 'connect');

    clientHandle.onread = function(nread, buffer) {
      // we're not expecting to ever get an EOF from the client.
      // just lots of data forever.
      if (nread < 0)
        fail(nread, 'read');

      const writeReq = new WriteWrap();
      writeReq.async = false;
      err = clientHandle.writeBuffer(writeReq, buffer);

      if (err)
        fail(err, 'write');

      writeReq.oncomplete = function(status, handle, req, err) {
        if (err)
          fail(err, 'write');
      };
    };

    clientHandle.readStart();
  };

  // Client
  var chunk;
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(len, 'x');
      break;
    case 'utf':
      chunk = 'ü'.repeat(len / 2);
      break;
    case 'asc':
      chunk = 'x'.repeat(len);
      break;
    default:
      throw new Error(`invalid type: ${type}`);
  }

  const clientHandle = new TCP(TCPConstants.SOCKET);
  const connectReq = new TCPConnectWrap();
  var bytes = 0;

  err = clientHandle.connect(connectReq, '127.0.0.1', PORT);
  if (err)
    fail(err, 'connect');

  clientHandle.onread = function(nread, buffer) {
    if (nread < 0)
      fail(nread, 'read');

    bytes += buffer.length;
  };

  connectReq.oncomplete = function(err) {
    if (err)
      fail(err, 'connect');

    bench.start();

    clientHandle.readStart();

    setTimeout(function() {
      // multiply by 2 since we're sending it first one way
      // then then back again.
      bench.end(2 * (bytes * 8) / (1024 * 1024 * 1024));
      process.exit(0);
    }, dur * 1000);

    while (clientHandle.writeQueueSize === 0)
      write();
  };

  function write() {
    const writeReq = new WriteWrap();
    writeReq.oncomplete = afterWrite;
    var err;
    switch (type) {
      case 'buf':
        err = clientHandle.writeBuffer(writeReq, chunk);
        break;
      case 'utf':
        err = clientHandle.writeUtf8String(writeReq, chunk);
        break;
      case 'asc':
        err = clientHandle.writeAsciiString(writeReq, chunk);
        break;
    }

    if (err)
      fail(err, 'write');
  }

  function afterWrite(err, handle, req) {
    if (err)
      fail(err, 'write');

    while (clientHandle.writeQueueSize === 0)
      write();
  }
}
