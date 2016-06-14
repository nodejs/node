'use strict';

var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  assert.throws(function() {
    res.writeHead(200, 'OK\r\nContent-Type: text/html\r\n');
  }, /Invalid character in statusMessage/);

  assert.throws(function() {
    res.writeHead(200, 'OK\u010D\u010AContent-Type: gotcha\r\n');
  }, /Invalid character in statusMessage/);
  res.end();
}).listen(common.PORT, common.mustCall(function() {
  var url = 'http://localhost:' + common.PORT;
  var left = 1;
  var check = common.mustCall(function(res) {
    res.resume();
    left--;
    assert.notEqual(res.headers['content-type'], 'text/html');
    assert.notEqual(res.headers['content-type'], 'gotcha');
    if (left === 0) server.close();
  }, 1);
  http.get(url + '/explicit', check);
}));
