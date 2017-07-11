'use strict';

var assert = require('assert');
var http = require('http');
var fs = require('fs');
var { fork } = require('child_process');
var common = require('../common.js');
const { PIPE, tmpDir } = require('../../test/common');
process.env.PIPE_NAME = PIPE;

try {
  fs.accessSync(tmpDir, fs.F_OK);
} catch (e) {
  fs.mkdirSync(tmpDir);
}

var server;
try {
  fs.unlinkSync(process.env.PIPE_NAME);
} catch (e) { /* ignore */ }

server = http.createServer(function(req, res) {
  var headers = {
    'content-type': 'text/plain',
    'content-length': '2'
  };
  res.writeHead(200, headers);
  res.end('ok');
});

server.on('error', function(err) {
  throw new Error(`server error: ${err}`);
});
server.listen(PIPE);

const child = fork(
  `${__dirname}/_chunky_http_client.js`,
  process.argv.slice(2)
);
child.on('message', common.sendResult);
child.on('close', function(code) {
  server.close();
  assert.strictEqual(code, 0);
});
