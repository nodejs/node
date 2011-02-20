var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var http = require('http');
var path = require('path');
var cp = require('child_process');

var filename = path.join(common.tmpDir || '/tmp', 'big');
var clientReqComplete = false;
var count = 0;

var server = http.createServer(function(req, res) {
  var timeoutId;
  assert.equal('POST', req.method);
  req.pause();
  common.error('request paused');

  setTimeout(function() {
    req.resume();
    common.error('request resumed');
  }, 1000);

  req.on('data', function(chunk) {
    common.error('recv data! nchars = ' + chunk.length);
    count += chunk.length;
  });

  req.on('end', function() {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end();
  });
});
server.listen(common.PORT);

server.on('listening', function() {
  var cmd = 'dd if=/dev/zero of="' + filename + '" bs=1024 count=10240';
  cp.exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    makeRequest();
  });
});

function makeRequest() {
  var req = http.request({
    port: common.PORT,
    path: '/',
    method: 'POST'
  });

  common.error('pipe!');
  var s = fs.ReadStream(filename);
  s.pipe(req);
  s.on('close', function(err) {
    if (err) throw err;
    clientReqComplete = true;
    common.error('client finished sending request');
  });

  req.on('response', function(res) {
    res.on('end', function() {
      server.close();
    });
  });
}

process.on('exit', function() {
  assert.equal(1024 * 10240, count);
  assert.ok(clientReqComplete);
});
