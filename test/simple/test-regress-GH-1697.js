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

var net = require('net'),
    cp = require('child_process'),
    util = require('util');

if (process.argv[2] === 'server') {
  // Server

  var server = net.createServer(function(conn) {
    conn.on('data', function(data) {
      console.log('server received ' + data.length + ' bytes');
    });

    conn.on('close', function() {
      server.close();
    });
  });
  server.listen(1234, '127.0.0.1');

  console.log('Server running.');
} else {
  // Client

  var serverProcess = cp.spawn(process.execPath, [process.argv[1], 'server']);
  serverProcess.stdout.pipe(process.stdout);
  serverProcess.stderr.pipe(process.stdout);

  serverProcess.stdout.once('data', function() {
    var client = net.createConnection(1234, '127.0.0.1');
    client.on('connect', function() {
      var alot = new Buffer(1024),
          alittle = new Buffer(1);

      for (var i = 0; i < 100; i++) {
        client.write(alot);
      }

      // Block the event loop for 1 second
      var start = (new Date()).getTime();
      while ((new Date).getTime() < start + 1000) {}

      client.write(alittle);

      client.destroySoon();
    });
  });
}
