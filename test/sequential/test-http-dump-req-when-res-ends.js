'use strict';

const common = require('../common');
const http = require('http');
const fs = require('fs');

const server = http.createServer(function(req, res) {
  // this checks if the request gets dumped
  req.on('resume', common.mustCall(function() {
    console.log('resume called');

    req.on('data', common.mustCallAtLeast(function(d) {
      console.log('data', d);
    }, 1));
  }));

  // end is not called as we are just exhausting
  // the in-memory buffer
  req.on('end', common.mustNotCall);

  // this 'data' handler will be removed when dumped
  req.on('data', common.mustNotCall);

  // start sending the response
  res.flushHeaders();

  setTimeout(function() {
    res.end('hello world');
  }, common.platformTimeout(100));
});

server.listen(0, function() {
  const req = http.request({
    method: 'POST',
    port: server.address().port
  });

  // Send the http request without waiting
  // for the body
  req.flushHeaders();

  req.on('response', common.mustCall(function(res) {
    // pipe the body as soon as we get the headers of the
    // response back
    fs.createReadStream(__filename).pipe(req);

    res.resume();

    // wait for the response
    res.on('end', function() {
      server.close();
    });
  }));
});
