'use strict';
const common = require('../common');
const http = require('http');
const OutgoingMessage = require('_http_outgoing').OutgoingMessage;

const server = http.createServer(common.mustCall(function(req, res) {
  console.log('Got a request, piping an OutgoingMessage to it.');
  const outgointMessage = new OutgoingMessage();
  outgointMessage.pipe(res);

  setTimeout(() => {
    console.log('Closing response.');
    res.end();
    outgointMessage.emit('data', 'some data');

    // If we got here - 'write after end' wasn't raised and the test passed.
    setTimeout(() => server.close(), 10);
  }, 10);

  setInterval(() => {
    console.log('Emitting some data to outgointMessage');
    outgointMessage.emit('data', 'some data');
  }, 1).unref();

}));

server.listen(0);

server.on('listening', common.mustCall(function() {
  http.request({
    port: server.address().port,
    method: 'GET',
    path: '/'
  }).end();
}));
