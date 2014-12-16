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
var http = require('http');
var assert = require('assert');

var server = http.Server(function(req, res) {
  console.log('Server accepted request.');
  res.writeHead(200);
  res.write('Part of my res.');

  res.destroy();
});

var responseClose = false;

server.listen(common.PORT, function() {
  var client = http.get({
    port: common.PORT,
    headers: { connection: 'keep-alive' }

  }, function(res) {
    server.close();

    console.log('Got res: ' + res.statusCode);
    console.dir(res.headers);

    res.on('data', function(chunk) {
      console.log('Read ' + chunk.length + ' bytes');
      console.log(' chunk=%j', chunk.toString());
    });

    res.on('end', function() {
      console.log('Response ended.');
    });

    res.on('aborted', function() {
      console.log('Response aborted.');
    });

    res.socket.on('close', function() {
      console.log('socket closed, but not res');
    })

    // it would be nice if this worked:
    res.on('close', function() {
      console.log('Response aborted');
      responseClose = true;
    });
  });
});


process.on('exit', function() {
  assert.ok(responseClose);
});
