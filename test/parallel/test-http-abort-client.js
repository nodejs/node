'use strict';
const common = require('../common');
const http = require('http');

const server = http.Server(function(req, res) {
  console.log('Server accepted request.');
  res.writeHead(200);
  res.write('Part of my res.');

  res.destroy();
});

server.listen(0, common.mustCall(function() {
  http.get({
    port: this.address().port,
    headers: { connection: 'keep-alive' }
  }, common.mustCall(function(res) {
    server.close();

    console.log('Got res: ' + res.statusCode);
    console.dir(res.headers);

    res.on('data', common.mustCall((chunk) => {
      console.log('Read ' + chunk.length + ' bytes');
      console.log(' chunk=%j', chunk.toString());
    }));

    res.on('end', common.mustCall(() => {
      console.log('Response ended.');
    }));

    res.on('aborted', common.mustCall(() => {
      console.log('Response aborted.');
    }));

    res.socket.on('close', common.mustCall(() => {
      console.log('socket closed, but not res');
    }));

    res.on('close', common.mustCall(() => {
      console.log('Response closed.');
    }));
  }));
}));
