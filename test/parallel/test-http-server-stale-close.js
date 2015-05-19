'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var util = require('util');
var fork = require('child_process').fork;

if (process.env.NODE_TEST_FORK) {
  var req = http.request({
    headers: {'Content-Length': '42'},
    method: 'POST',
    host: '127.0.0.1',
    port: common.PORT,
  }, process.exit);
  req.write('BAM');
  req.end();
}
else {
  var server = http.createServer(function(req, res) {
    res.writeHead(200, {'Content-Length': '42'});
    req.pipe(res);
    req.on('close', function() {
      server.close();
      res.end();
    });
  });
  server.listen(common.PORT, function() {
    fork(__filename, {
      env: util._extend(process.env, {NODE_TEST_FORK: '1'})
    });
  });
}
