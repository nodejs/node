'use strict';
require('../common');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();

  server.close();
});

server.listen(0, function() {

  const req = http.request({
    method: 'POST',
    port: this.address().port
  });

  const payload = Buffer.alloc(16390, 'Й');
  req.write(payload);
  req.end();
});
