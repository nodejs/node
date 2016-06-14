'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var MAX_REQUESTS = 12;
var reqNum = 0;

var server = http.Server(common.mustCall(function(req, res) {
  switch (reqNum) {
    case 0:
      assert.throws(common.mustCall(function() {
        res.writeHead(-1);
      }, /invalid status code/i));
      break;
    case 1:
      assert.throws(common.mustCall(function() {
        res.writeHead(Infinity);
      }, /invalid status code/i));
      break;
    case 2:
      assert.throws(common.mustCall(function() {
        res.writeHead(NaN);
      }, /invalid status code/i));
      break;
    case 3:
      assert.throws(common.mustCall(function() {
        res.writeHead({});
      }, /invalid status code/i));
      break;
    case 4:
      assert.throws(common.mustCall(function() {
        res.writeHead(99);
      }, /invalid status code/i));
      break;
    case 5:
      assert.throws(common.mustCall(function() {
        res.writeHead(1000);
      }, /invalid status code/i));
      break;
    case 6:
      assert.throws(common.mustCall(function() {
        res.writeHead('1000');
      }, /invalid status code/i));
      break;
    case 7:
      assert.throws(common.mustCall(function() {
        res.writeHead(null);
      }, /invalid status code/i));
      break;
    case 8:
      assert.throws(common.mustCall(function() {
        res.writeHead(true);
      }, /invalid status code/i));
      break;
    case 9:
      assert.throws(common.mustCall(function() {
        res.writeHead([]);
      }, /invalid status code/i));
      break;
    case 10:
      assert.throws(common.mustCall(function() {
        res.writeHead('this is not valid');
      }, /invalid status code/i));
      break;
    case 11:
      assert.throws(common.mustCall(function() {
        res.writeHead('404 this is not valid either');
      }, /invalid status code/i));
      this.close();
      break;
    default:
      throw new Error('Unexpected request');
  }
  res.statusCode = 200;
  res.end();
}, MAX_REQUESTS));
server.listen();

server.on('listening', function makeRequest() {
  var self = this;
  http.get({
    port: self.address().port
  }, function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', function() {
      if (++reqNum < MAX_REQUESTS)
        makeRequest.call(self);
    });
    res.resume();
  });
});
