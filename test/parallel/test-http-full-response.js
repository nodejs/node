'use strict';
var common = require('../common');
var assert = require('assert');
// This test requires the program 'ab'
var http = require('http');
var exec = require('child_process').exec;

var bodyLength = 12345;

var body = 'c'.repeat(bodyLength);

var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': bodyLength,
    'Content-Type': 'text/plain'
  });
  res.end(body);
});

function runAb(opts, callback) {
  var command = `ab ${opts} http://127.0.0.1:${server.address().port}/`;
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (/ab|apr/mi.test(stderr)) {
        common.skip('problem spawning `ab`.\n' + stderr);
        process.reallyExit(0);
      }
      process.exit();
      return;
    }

    var m = /Document Length:\s*(\d+) bytes/mi.exec(stdout);
    var documentLength = parseInt(m[1]);

    m = /Complete requests:\s*(\d+)/mi.exec(stdout);
    var completeRequests = parseInt(m[1]);

    m = /HTML transferred:\s*(\d+) bytes/mi.exec(stdout);
    var htmlTransfered = parseInt(m[1]);

    assert.equal(bodyLength, documentLength);
    assert.equal(completeRequests * documentLength, htmlTransfered);

    if (callback) callback();
  });
}

server.listen(0, common.mustCall(function() {
  runAb('-c 1 -n 10', common.mustCall(function() {
    console.log('-c 1 -n 10 okay');

    runAb('-c 1 -n 100', common.mustCall(function() {
      console.log('-c 1 -n 100 okay');

      runAb('-c 1 -n 1000', common.mustCall(function() {
        console.log('-c 1 -n 1000 okay');
        server.close();
      }));
    }));
  }));
}));
