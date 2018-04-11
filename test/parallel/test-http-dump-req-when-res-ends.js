'use strict';

const { mustCall } = require('../common');

const fs = require('fs');
const http = require('http');
const { strictEqual } = require('assert');

const server = http.createServer(mustCall(function(req, res) {
  strictEqual(req.socket.listenerCount('data'), 1);
  req.socket.once('data', mustCall(function() {
    // Ensure that a chunk of data is received before calling `res.end()`.
    res.end('hello world');
  }));
  // This checks if the request gets dumped
  // resume will be triggered by res.end().
  req.on('resume', mustCall(function() {
    // There is no 'data' event handler anymore
    // it gets automatically removed when dumping the request.
    strictEqual(req.listenerCount('data'), 0);
    req.on('data', mustCall());
  }));

  // We explicitly pause the stream
  // so that the following on('data') does not cause
  // a resume.
  req.pause();
  req.on('data', function() {});

  // Start sending the response.
  res.flushHeaders();
}));

server.listen(0, mustCall(function() {
  const req = http.request({
    method: 'POST',
    port: server.address().port
  });

  // Send the http request without waiting
  // for the body.
  req.flushHeaders();

  req.on('response', mustCall(function(res) {
    // Pipe the body as soon as we get the headers of the
    // response back.
    fs.createReadStream(__filename).pipe(req);

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
