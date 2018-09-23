'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

http.createServer(function(req, res) {
  res.end('ok\n');
  this.close();
}).listen(common.PORT, test);

function test() {
  http.request({
    port: common.PORT,
    encoding: 'utf8'
  }, function(res) {
    res.pipe(process.stdout);
  }).end();
}
