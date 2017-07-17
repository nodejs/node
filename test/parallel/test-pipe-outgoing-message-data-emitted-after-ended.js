'use strict';
const common = require('../common');
const http = require('http');
const util = require('util');
const stream = require('stream');

// Verify that when piping a stream to an `OutgoingMessage` (or a type that
// inherits from `OutgoingMessage`), if data is emitted after the
// `OutgoingMessage` was closed - no `write after end` error is raised (this
// should be the case when piping - when writing data directly to the
// `OutgoingMessage` this error should be raised).

function MyStream() {
  stream.call(this);
}
util.inherits(MyStream, stream);

const server = http.createServer(common.mustCall(function(req, res) {
  const myStream = new MyStream();
  myStream.pipe(res);

  process.nextTick(common.mustCall(() => {
    res.end();
    myStream.emit('data', 'some data');

    // If we got here - 'write after end' wasn't raised and the test passed.
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
