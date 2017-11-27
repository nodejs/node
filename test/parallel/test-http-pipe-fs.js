'use strict';
const common = require('../common');
const http = require('http');
const fs = require('fs');
const path = require('path');
const Countdown = require('../common/countdown');
const NUMBER_OF_STREAMS = 2;

const countdown = new Countdown(NUMBER_OF_STREAMS, () => server.close());

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'http-pipe-fs-test.txt');

const server = http.createServer(common.mustCall(function(req, res) {
  const stream = fs.createWriteStream(file);
  req.pipe(stream);
  stream.on('close', function() {
    res.writeHead(200);
    res.end();
  });
}, 2)).listen(0, function() {
  http.globalAgent.maxSockets = 1;

  for (let i = 0; i < NUMBER_OF_STREAMS; ++i) {
    const req = http.request({
      port: server.address().port,
      method: 'POST',
      headers: {
        'Content-Length': 5
      }
    }, function(res) {
      res.on('end', function() {
        console.error(`res${i + 1} end`);
        countdown.dec();
      });
      res.resume();
    });
    req.on('socket', function(s) {
      console.error(`req${i + 1} start`);
    });
    req.end('12345');
  }
});
