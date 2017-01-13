'use strict';
require('../common');
const http = require('http');
const util = require('util');
const fork = require('child_process').fork;

if (process.env.NODE_TEST_FORK_PORT) {
  const req = http.request({
    headers: {'Content-Length': '42'},
    method: 'POST',
    host: '127.0.0.1',
    port: +process.env.NODE_TEST_FORK_PORT,
  }, process.exit);
  req.write('BAM');
  req.end();
} else {
  const server = http.createServer(function(req, res) {
    res.writeHead(200, {'Content-Length': '42'});
    req.pipe(res);
    req.on('close', function() {
      server.close();
      res.end();
    });
  });
  server.listen(0, function() {
    fork(__filename, {
      env: util._extend(process.env, {NODE_TEST_FORK_PORT: this.address().port})
    });
  });
}
