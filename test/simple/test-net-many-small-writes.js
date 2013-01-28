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

// make sure we get no tickDepth warnings.

if (process.argv[2] === 'child')
  child();
else
  parent();

function parent() {
  var spawn = require('child_process').spawn;
  var node = process.execPath;
  var child = spawn(node, [__filename, 'child']);

  child.stdout.pipe(process.stdout);

  child.stderr.setEncoding('utf8');
  var err = '';
  child.stderr.on('data', function(c) {
    err += c;
  });
  var gotClose = false;
  child.on('close', function(code) {
    assert(!code);
    assert.equal(err, '');
    gotClose = true;
  });
  process.on('exit', function() {
    assert(gotClose);
    console.log('PARENT: ok');
  });
}

function child() {
  var net = require('net');
  process.maxTickDepth = 10;
  var server = net.createServer(function(sock) {
    var i = 0;
    w();
    function w() {
      if (++i < 100)
        sock.write(new Buffer(1), w);
      else
        sock.end();
    }
  });
  server.listen(common.PORT);

  var finished = false;
  var client = net.connect(common.PORT);
  client.resume();
  client.on('end', function() {
    server.close(function() {
      finished = true;
    });
  });

  process.on('exit', function() {
    assert(finished);
    console.log('CHILD: ok');
  });
}
