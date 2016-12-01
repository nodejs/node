'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const http = require('http');
const path = require('path');
const cp = require('child_process');

common.refreshTmpDir();

const filename = path.join(common.tmpDir || '/tmp', 'big');
let count = 0;

const server = http.createServer(function(req, res) {
  let timeoutId;
  assert.strictEqual('POST', req.method);
  req.pause();

  setTimeout(function() {
    req.resume();
  }, 1000);

  req.on('data', function(chunk) {
    count += chunk.length;
  });

  req.on('end', function() {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end();
  });
});
server.listen(0);

server.on('listening', function() {
  const cmd = common.ddCommand(filename, 10240);

  cp.exec(cmd, function(err) {
    if (err) throw err;
    makeRequest();
  });
});

function makeRequest() {
  const req = http.request({
    port: server.address().port,
    path: '/',
    method: 'POST'
  });

  const s = fs.ReadStream(filename);
  s.pipe(req);
  s.on('close', common.mustCall((err) => {
    assert.ifError(err);
  }));

  req.on('response', function(res) {
    res.resume();
    res.on('end', function() {
      server.close();
    });
  });
}

process.on('exit', function() {
  assert.strictEqual(1024 * 10240, count);
});
