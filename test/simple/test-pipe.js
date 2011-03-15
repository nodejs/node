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
var http = require('http');
var net = require('net');

var webPort = common.PORT;
var tcpPort = webPort + 1;

var listenCount = 0;
var gotThanks = false;
var tcpLengthSeen = 0;
var bufferSize = 5 * 1024 * 1024;


/*
 * 5MB of random buffer.
 */
var buffer = Buffer(bufferSize);
for (var i = 0; i < buffer.length; i++) {
  buffer[i] = parseInt(Math.random() * 10000) % 256;
}


var web = http.Server(function(req, res) {
  web.close();

  console.log(req.headers);

  var socket = net.Stream();
  socket.connect(tcpPort);

  socket.on('connect', function() {
    console.log('socket connected');
  });

  req.pipe(socket);

  req.on('end', function() {
    res.writeHead(200);
    res.write('thanks');
    res.end();
    console.log('response with \'thanks\'');
  });

  req.connection.on('error', function(e) {
    console.log('http server-side error: ' + e.message);
    process.exit(1);
  });
});
web.listen(webPort, startClient);



var tcp = net.Server(function(s) {
  tcp.close();

  console.log('tcp server connection');

  var i = 0;

  s.on('data', function(d) {
    process.stdout.write('.');
    tcpLengthSeen += d.length;
    for (var j = 0; j < d.length; j++) {
      assert.equal(buffer[i], d[j]);
      i++;
    }
  });

  s.on('end', function() {
    console.log('tcp socket disconnect');
    s.end();
  });

  s.on('error', function(e) {
    console.log('tcp server-side error: ' + e.message);
    process.exit(1);
  });
});
tcp.listen(tcpPort, startClient);


function startClient() {
  listenCount++;
  if (listenCount < 2) return;

  console.log('Making request');

  var client = http.createClient(common.PORT);
  var req = client.request('GET', '/', { 'content-length': buffer.length });
  req.write(buffer);
  req.end();

  req.on('response', function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', function(string) {
      assert.equal('thanks', string);
      gotThanks = true;
    });
  });
}

process.on('exit', function() {
  assert.ok(gotThanks);
  assert.equal(bufferSize, tcpLengthSeen);
});

