// In this benchmark, we connect a client to the server, and write
// as many bytes as we can in the specified time (default = 10s)

var common = require('../common.js');

// if there are --dur=N and --len=N args, then
// run the function with those settings.
// if not, then queue up a bunch of child processes.
var bench = common.createBenchmark(main, {
  len: [102400, 1024 * 1024 * 16],
  type: ['utf', 'asc', 'buf'],
  dur: [5]
});

var TCP = process.binding('tcp_wrap').TCP;
var PORT = common.PORT;

var dur;
var len;
var type;

function main(conf) {
  dur = +conf.dur;
  len = +conf.len;
  type = conf.type;
  server();
}


function fail(syscall) {
  var e = new Error(syscall + ' ' + errno);
  e.errno = e.code = errno;
  e.syscall = syscall;
  throw e;
}

function server() {
  var serverHandle = new TCP();
  var r = serverHandle.bind('127.0.0.1', PORT);
  if (r)
    fail('bind');

  var r = serverHandle.listen(511);
  if (r)
    fail('listen');

  serverHandle.onconnection = function(clientHandle) {
    if (!clientHandle)
      fail('connect');

    clientHandle.onread = function(buffer, offset, length) {
      // we're not expecting to ever get an EOF from the client.
      // just lots of data forever.
      if (!buffer)
        fail('read');

      var chunk = buffer.slice(offset, offset + length);
      var writeReq = clientHandle.writeBuffer(chunk);

      if (!writeReq)
        fail('write');

      writeReq.oncomplete = function(status, handle, req) {
        if (status)
          fail('write');
      };
    };

    clientHandle.readStart();
  };

  client();
}

function client() {
  var chunk;
  switch (type) {
    case 'buf':
      chunk = new Buffer(len);
      chunk.fill('x');
      break;
    case 'utf':
      chunk = new Array(len / 2 + 1).join('ü');
      break;
    case 'asc':
      chunk = new Array(len + 1).join('x');
      break;
    default:
      throw new Error('invalid type: ' + type);
      break;
  }

  var clientHandle = new TCP();
  var connectReq = clientHandle.connect('127.0.0.1', PORT);
  var bytes = 0;

  if (!connectReq)
    fail('connect');

  clientHandle.readStart();

  clientHandle.onread = function(buffer, start, length) {
    if (!buffer)
      fail('read');

    bytes += length;
  };

  connectReq.oncomplete = function() {
    bench.start();

    setTimeout(function() {
      // multiply by 2 since we're sending it first one way
      // then then back again.
      bench.end(2 * (bytes * 8) / (1024 * 1024 * 1024));
    }, dur * 1000);

    while (clientHandle.writeQueueSize === 0)
      write();
  };

  function write() {
    var writeReq
    switch (type) {
      case 'buf':
        writeReq = clientHandle.writeBuffer(chunk);
        break;
      case 'utf':
        writeReq = clientHandle.writeUtf8String(chunk);
        break;
      case 'asc':
        writeReq = clientHandle.writeAsciiString(chunk);
        break;
    }

    if (!writeReq)
      fail('write');

    writeReq.oncomplete = afterWrite;
  }

  function afterWrite(status, handle, req) {
    if (status)
      fail('write');

    while (clientHandle.writeQueueSize === 0)
      write();
  }
}
