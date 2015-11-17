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
//
// Flags: --allow-insecure-server-dhparam

var common = require('../common');

var assert = require('assert');
var tls = require('tls');
var fs = require('fs');
var key =  fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem');

function loadDHParam(n) {
  var path = common.fixturesDir;
  if (n !== 'error') path += '/keys';
  return fs.readFileSync(path + '/dh' + n + '.pem');
}

// validate that the server will accept a key smaller than
// 768 provided the required command line option is specified
// the flags statement above ensures that the test harnesss
// runs the test with the required command line option
function test512() {
  var options = {
    key: key,
    cert: cert,
    dhparam: loadDHParam(512)
  };

  var server = tls.createServer(options)
}

test512();
