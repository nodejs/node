'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

let testsComplete = 0;

const testCases = [
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
  const matching = this.filter(function(testCase) {
    return testCase.path === path;
  });
  if (matching.length === 0) {
    common.fail(`failed to find test case with path ${path}`);
  }
  return matching[0];
};

const server = net.createServer(function(connection) {
  connection.on('data', function(data) {
    const path = data.toString().match(/GET (.*) HTTP.1.1/)[1];
    const testCase = testCases.findByPath(path);

    connection.write(testCase.response);
    connection.end();
  });
});

const runTest = function(testCaseIndex) {
  const testCase = testCases[testCaseIndex];

  http.get({
    port: server.address().port,
    path: testCase.path
  }, function(response) {
    console.log('client: expected status message: ' + testCase.statusMessage);
    console.log('client: actual status message: ' + response.statusMessage);
    assert.strictEqual(testCase.statusMessage, response.statusMessage);

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
  assert.strictEqual(testCases.length, testsComplete);
});
