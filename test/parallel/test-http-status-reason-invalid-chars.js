'use strict';

var common = require('../common');
var assert = require('assert');
var http = require('http');

function explicit(req, res) {
  assert.throws(function() {
    res.writeHead(200, 'OK\r\nContent-Type: text/html\r\n');
  }, /Invalid character in statusMessage/);

  assert.throws(function() {
    res.writeHead(200, 'OK\u010D\u010AContent-Type: gotcha\r\n');
  }, /Invalid character in statusMessage/);

  res.statusMessage = 'OK';
  res.end();
}

function implicit(req, res) {
  assert.throws(function() {
    res.statusMessage = 'OK\r\nContent-Type: text/html\r\n';
    res.writeHead(200);
  }, /Invalid character in statusMessage/);
  res.statusMessage = 'OK';
  res.end();
}

var server = http.createServer(function(req, res) {
  if (req.url === '/explicit') {
    explicit(req, res);
  } else {
    implicit(req, res);
  }
}).listen(common.PORT, common.mustCall(function() {
  var url = 'http://localhost:' + common.PORT;
  var left = 2;
  var check = common.mustCall(function(res) {
    left--;
    assert.notEqual(res.headers['content-type'], 'text/html');
    assert.notEqual(res.headers['content-type'], 'gotcha');
    if (left === 0) server.close();
  }, 2);
  http.get(url + '/explicit', check).end();
  http.get(url + '/implicit', check).end();
}));
