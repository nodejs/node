'use strict';
const common = require('../common');
const assert = require('assert');
// This test requires the program 'ab'
const http = require('http');
const exec = require('child_process').exec;

const bodyLength = 12345;

const body = 'c'.repeat(bodyLength);

const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': bodyLength,
    'Content-Type': 'text/plain'
  });
  res.end(body);
});

function runAb(opts, callback) {
  const command = `ab ${opts} http://127.0.0.1:${server.address().port}/`;
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (/ab|apr/mi.test(stderr)) {
        common.skip('problem spawning `ab`.\n' + stderr);
        process.reallyExit(0);
      }
      process.exit();
      return;
    }

    let m = /Document Length:\s*(\d+) bytes/mi.exec(stdout);
    const documentLength = parseInt(m[1]);

    m = /Complete requests:\s*(\d+)/mi.exec(stdout);
    const completeRequests = parseInt(m[1]);

    m = /HTML transferred:\s*(\d+) bytes/mi.exec(stdout);
    const htmlTransfered = parseInt(m[1]);

    assert.strictEqual(bodyLength, documentLength);
    assert.strictEqual(completeRequests * documentLength, htmlTransfered);

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
