'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const fs = require('fs');
const util = require('util');
const path = require('path');
const fn = path.join(common.fixturesDir, 'does_not_exist.txt');

let got_error = false;
let conn_closed = false;

const server = net.createServer(function(stream) {
  util.pump(fs.createReadStream(fn), stream, function(err) {
    if (err) {
      got_error = true;
    } else {
      // util.pump's callback fired with no error
      // this shouldn't happen as the file doesn't exist...
      assert.equal(true, false);
    }
    server.close();
  });
});

server.listen(common.PORT, function() {
  const conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');

  conn.on('end', function() {
    conn.end();
  });

  conn.on('close', function() {
    conn_closed = true;
  });
});

process.on('exit', function() {
  assert.equal(true, got_error);
  assert.equal(true, conn_closed);
});
