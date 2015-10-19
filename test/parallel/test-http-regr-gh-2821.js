'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();

  server.close();
});

server.listen(common.PORT, function() {

  const req = http.request({
    method: 'POST',
    port: common.PORT
  });

  const payload = new Buffer(16390);
  payload.fill('Й');
  req.write(payload);
  req.end();
});
