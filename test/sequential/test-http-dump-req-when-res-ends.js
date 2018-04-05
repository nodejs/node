'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const fs = require('fs');

let resEnd = null;

const server = http.createServer(common.mustCall(function(req, res) {
  // This checks if the request gets dumped
  // resume will be triggered by res.end().
  req.on('resume', common.mustCall(function() {
    // There is no 'data' event handler anymore
    // it gets automatically removed when dumping the request.
    assert.strictEqual(req.listenerCount('data'), 0);

    req.on('data', common.mustCallAtLeast(function(d) {
      // Leaving the console.log explicitly, so that
      // we can know how many chunks we have received.
      console.log('data', d);
    }, 1));
  }));

  // We explicitly pause the stream
  // so that the following on('data') does not cause
  // a resume.
  req.pause();
  req.on('data', function() {});

  // Start sending the response.
  res.flushHeaders();

  resEnd = function() {
    setImmediate(function() {
      res.end('hello world');
    });
  };
}));

server.listen(0, common.mustCall(function() {
  const req = http.request({
    method: 'POST',
    port: server.address().port
  });

  // Send the http request without waiting
  // for the body.
  req.flushHeaders();

  req.on('response', common.mustCall(function(res) {
    // Pipe the body as soon as we get the headers of the
    // response back.
    const readFileStream = fs.createReadStream(__filename);
    readFileStream.on('end', resEnd);

    readFileStream.pipe(req);

    res.resume();

    // Wait for the response.
    res.on('end', function() {
      server.close();
    });
  }));

  req.on('error', function() {
    // An error can happen if there is some data still
    // being sent, as the other side is calling .destroy()
    // this is safe to ignore.
  });
}));
