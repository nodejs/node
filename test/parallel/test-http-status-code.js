'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

// Simple test of Node's HTTP ServerResponse.statusCode
// ServerResponse.prototype.statusCode

var testsComplete = 0;
var tests = [200, 202, 300, 404, 451, 500];
var testIdx = 0;

var s = http.createServer(function(req, res) {
  var t = tests[testIdx];
  res.writeHead(t, {'Content-Type': 'text/plain'});
  console.log('--\nserver: statusCode after writeHead: ' + res.statusCode);
  assert.equal(res.statusCode, t);
  res.end('hello world\n');
});

s.listen(0, nextTest);


function nextTest() {
  if (testIdx + 1 === tests.length) {
    return s.close();
  }
  var test = tests[testIdx];

  http.get({ port: s.address().port }, function(response) {
    console.log('client: expected status: ' + test);
    console.log('client: statusCode: ' + response.statusCode);
    assert.equal(response.statusCode, test);
    response.on('end', function() {
      testsComplete++;
      testIdx += 1;
      nextTest();
    });
    response.resume();
  });
}


process.on('exit', function() {
  assert.equal(5, testsComplete);
});

