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




// This must be run as root.

var http = require('http'),
    https = require('https'),
    PORT = 80,
    SSLPORT = 443,
    assert = require('assert'),
    hostExpect = 'localhost',
    fs = require('fs'),
    path = require('path'),
    fixtures = path.resolve(__dirname, '../fixtures/keys'),
    options = {
      key: fs.readFileSync(fixtures + '/agent1-key.pem'),
      cert: fs.readFileSync(fixtures + '/agent1-cert.pem')
    };

http.createServer(function(req, res) {
  console.error(req.headers);
  assert.equal(req.headers.host, hostExpect);
  res.writeHead(200);
  res.end('ok');
  this.close();
}).listen(PORT);

https.createServer(options, function(req, res) {
  console.error(req.headers);
  assert.equal(req.headers.host, hostExpect);
  res.writeHead(200);
  res.end('ok');
  this.close();
}).listen(SSLPORT);

http
  .get({ host: 'localhost',
      port: PORT,
      headers: { 'x-port': PORT } })
  .on('response', function(res) {});

https
  .get({ host: 'localhost',
      port: SSLPORT,
      headers: { 'x-port': SSLPORT } })
  .on('response', function(res) {});
