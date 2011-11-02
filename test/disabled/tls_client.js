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

var util = require('util');
var net = require('net');
var fs = require('fs');
var crypto = require('crypto');

//var client = net.createConnection(4443, 'localhost');
var client = net.createConnection(443, 'www.microsoft.com');
//var client = net.createConnection(443, 'www.google.com');

var caPem = fs.readFileSync(common.fixturesDir + '/msca.pem');
//var caPem = fs.readFileSync('ca.pem');

try {
  var credentials = crypto.createCredentials({ca: caPem});
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

client.setEncoding('UTF8');
client.on('connect', function() {
  console.log('client connected.');
  client.setSecure(credentials);
});

client.on('secure', function() {
  console.log('client secure : ' + JSON.stringify(client.getCipher()));
  console.log(JSON.stringify(client.getPeerCertificate()));
  console.log('verifyPeer : ' + client.verifyPeer());
  client.write('GET / HTTP/1.0\r\n\r\n');
});

client.on('data', function(chunk) {
  common.error(chunk);
});

client.on('end', function() {
  console.log('client disconnected.');
});

