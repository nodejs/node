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

'use strict';
const common = require('../common');
const assert = require('assert');

const util = require('util');
const net = require('net');
const fs = require('fs');
const crypto = require('crypto');

var keyPem = fs.readFileSync(common.fixturesDir + '/cert.pem');
var certPem = fs.readFileSync(common.fixturesDir + '/cert.pem');

try {
  var credentials = crypto.createCredentials({key: keyPem, cert: certPem});
} catch (e) {
  common.skip('node compiled without OpenSSL.');
  return;
}
var i = 0;
var server = net.createServer(function(connection) {
  connection.setSecure(credentials);
  connection.setEncoding('latin1');

  connection.on('secure', function() {
    //console.log('Secure');
  });

  connection.on('data', function(chunk) {
    console.log('recved: ' + JSON.stringify(chunk));
    connection.write('HTTP/1.0 200 OK\r\n' +
                     'Content-type: text/plain\r\n' +
                     'Content-length: 9\r\n' +
                     '\r\n' +
                     'OK : ' + i +
                     '\r\n\r\n');
    i = i + 1;
    connection.end();
  });

  connection.on('end', function() {
    connection.end();
  });

});
server.listen(4443);


