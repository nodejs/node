'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const fs = require('fs');
const util = require('util');
const path = require('path');
const fn = path.join(common.fixturesDir, 'elipses.txt');

const expected = fs.readFileSync(fn, 'utf8');

const server = net.createServer(function(stream) {
  util.pump(fs.createReadStream(fn), stream, function() {
    server.close();
  });
});

server.listen(common.PORT, function() {
  const conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  conn.on('data', function(chunk) {
    buffer += chunk;
  });

  conn.on('end', function() {
    conn.end();
  });
});

let buffer = '';

server.on('listening', function() {
});

process.on('exit', function() {
  assert.equal(expected, buffer);
});
