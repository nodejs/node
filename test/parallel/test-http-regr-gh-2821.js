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

  const payload = new Array(1640).join('0123456789');
  req.write(payload);
  req.end();
});
