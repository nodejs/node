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
var tls = require('tls');
var net = require('net');
var fs = require('fs');
var assert = require('assert');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

var bonkers = new Buffer(1024 * 1024);
bonkers.fill(42);

var server = tls.createServer(options, function(c) {

}).listen(common.PORT, function() {
  var client = net.connect(common.PORT, function() {
    client.write(bonkers);
  });

  var once = false;

  var writeAgain = setTimeout(function() {
    client.write(bonkers);
  });

  client.on('error', function(err) {
    if (!once) {
      clearTimeout(writeAgain);
      once = true;
      client.destroy();
      server.close();
    }
  });

  client.on('close', function (hadError) {
    assert.strictEqual(hadError, true, 'Client never errored');
  });
});
