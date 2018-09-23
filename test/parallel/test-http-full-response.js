'use strict';
var common = require('../common');
var assert = require('assert');
// This test requires the program 'ab'
var http = require('http');
var exec = require('child_process').exec;

var bodyLength = 12345;

var body = '';
for (var i = 0; i < bodyLength; i++) {
  body += 'c';
}

var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': bodyLength,
    'Content-Type': 'text/plain'
  });
  res.end(body);
});

var runs = 0;

function runAb(opts, callback) {
  var command = 'ab ' + opts + ' http://127.0.0.1:' + common.PORT + '/';
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (/ab|apr/mi.test(stderr)) {
        console.log('1..0 # Skipped: problem spawning `ab`.\n' + stderr);
        process.reallyExit(0);
      }
      process.exit();
      return;
    }

    var m = /Document Length:\s*(\d+) bytes/mi.exec(stdout);
    var documentLength = parseInt(m[1]);

    var m = /Complete requests:\s*(\d+)/mi.exec(stdout);
    var completeRequests = parseInt(m[1]);

    var m = /HTML transferred:\s*(\d+) bytes/mi.exec(stdout);
    var htmlTransfered = parseInt(m[1]);

    assert.equal(bodyLength, documentLength);
    assert.equal(completeRequests * documentLength, htmlTransfered);

    runs++;

    if (callback) callback();
  });
}

server.listen(common.PORT, function() {
  runAb('-c 1 -n 10', function() {
    console.log('-c 1 -n 10 okay');

    runAb('-c 1 -n 100', function() {
      console.log('-c 1 -n 100 okay');

      runAb('-c 1 -n 1000', function() {
        console.log('-c 1 -n 1000 okay');
        server.close();
      });
    });
  });

});

process.on('exit', function() {
  assert.equal(3, runs);
});
