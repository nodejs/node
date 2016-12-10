'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var testsComplete = 0;

var testCases = [
  { path: '/200', statusMessage: 'OK',
    response: 'HTTP/1.1 200 OK\r\n\r\n' },
  { path: '/500', statusMessage: 'Internal Server Error',
    response: 'HTTP/1.1 500 Internal Server Error\r\n\r\n' },
  { path: '/302', statusMessage: 'Moved Temporarily',
    response: 'HTTP/1.1 302 Moved Temporarily\r\n\r\n' },
  { path: '/missing', statusMessage: '',
    response: 'HTTP/1.1 200 \r\n\r\n' },
  { path: '/missing-no-space', statusMessage: '',
    response: 'HTTP/1.1 200\r\n\r\n' }
];
testCases.findByPath = function(path) {
  var matching = this.filter(function(testCase) {
    return testCase.path === path;
  });
  if (matching.length === 0) {
    throw 'failed to find test case with path ' + path;
  }
  return matching[0];
};

var server = net.createServer(function(connection) {
  connection.on('data', function(data) {
    var path = data.toString().match(/GET (.*) HTTP.1.1/)[1];
    var testCase = testCases.findByPath(path);

    connection.write(testCase.response);
    connection.end();
  });
});

var runTest = function(testCaseIndex) {
  var testCase = testCases[testCaseIndex];

  http.get({
    port: server.address().port,
    path: testCase.path
  }, function(response) {
    console.log('client: expected status message: ' + testCase.statusMessage);
    console.log('client: actual status message: ' + response.statusMessage);
    assert.equal(testCase.statusMessage, response.statusMessage);

    response.on('end', function() {
      testsComplete++;

      if (testCaseIndex + 1 < testCases.length) {
        runTest(testCaseIndex + 1);
      } else {
        server.close();
      }
    });

    response.resume();
  });
};

server.listen(0, function() { runTest(0); });

process.on('exit', function() {
  assert.equal(testCases.length, testsComplete);
});
