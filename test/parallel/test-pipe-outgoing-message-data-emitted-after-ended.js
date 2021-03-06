'use strict';
const common = require('../common');
const http = require('http');
const stream = require('stream');

// Verify that when piping a stream to an `OutgoingMessage` (or a type that
// inherits from `OutgoingMessage`), if data is emitted after the
// `OutgoingMessage` was closed - a `write after end` error is raised

class MyStream extends stream {}

const server = http.createServer(common.mustCall(function(req, res) {
  const myStream = new MyStream();
  myStream.pipe(res);

  process.nextTick(common.mustCall(() => {
    res.end();
    myStream.emit('data', 'some data');
    res.on('error', common.expectsError({
      code: 'ERR_STREAM_WRITE_AFTER_END',
      name: 'Error'
    }));

    process.nextTick(common.mustCall(() => server.close()));
  }));
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  }).end();
}));
