'use strict';
const common = require('../common');
const http = require('http');
const util = require('util');
const stream = require('stream');

function MyStream() {
  stream.call(this);
}
util.inherits(MyStream, stream);

const server = http.createServer(common.mustCall(function(req, res) {
  console.log('Got a request, piping a stream to it.');
  const myStream = new MyStream();
  myStream.pipe(res);

  process.nextTick(() => {
    console.log('Closing response.');
    res.end();
    myStream.emit('data', 'some data');

    // If we got here - 'write after end' wasn't raised and the test passed.
    process.nextTick(() => server.close());
  });
}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  }).end();
}));
