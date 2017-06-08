
'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

var req;

process.on('exit', function() {
  assert.equal(req.url, '/?тест');
  assert.equal(req.headers['x-test'], 'тест');
});


http.createServer(function(req_, res) {
  req = req_;
  res.end();
  this.close();
}).listen(common.PORT, function() {
  http.request({
    port: common.PORT,
    path: new Buffer('/?тест').toString('binary'),
    headers: {
      'x-test': new Buffer('тест').toString('binary')
    }
   }).end();
});
