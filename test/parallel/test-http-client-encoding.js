'use strict';
require('../common');

var http = require('http');

http.createServer(function(req, res) {
  res.end('ok\n');
  this.close();
}).listen(0, test);

function test() {
  http.request({
    port: this.address().port,
    encoding: 'utf8'
  }, function(res) {
    res.pipe(process.stdout);
  }).end();
}
