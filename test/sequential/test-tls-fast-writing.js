// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var tls = require('tls');
var fs = require('fs');

var PORT = common.PORT;
var dir = common.fixturesDir;
var options = { key: fs.readFileSync(dir + '/test_key.pem'),
                cert: fs.readFileSync(dir + '/test_cert.pem'),
                ca: [ fs.readFileSync(dir + '/test_ca.pem') ] };

var server = tls.createServer(options, onconnection);
var gotChunk = false;
var gotDrain = false;

var timer = setTimeout(function() {
  console.log('not ok - timed out');
  process.exit(1);
}, 500);

function onconnection(conn) {
  conn.on('data', function(c) {
    if (!gotChunk) {
      gotChunk = true;
      console.log('ok - got chunk');
    }

    // just some basic sanity checks.
    assert(c.length);
    assert(Buffer.isBuffer(c));

    if (gotDrain)
      process.exit(0);
  });
}

server.listen(PORT, function() {
  var chunk = new Buffer(1024);
  chunk.fill('x');
  var opt = { port: PORT, rejectUnauthorized: false };
  var conn = tls.connect(opt, function() {
    conn.on('drain', ondrain);
    write();
  });
  function ondrain() {
    if (!gotDrain) {
      gotDrain = true;
      console.log('ok - got drain');
    }
    if (gotChunk)
      process.exit(0);
    write();
  }
  function write() {
    // this needs to return false eventually
    while (false !== conn.write(chunk));
  }
});
