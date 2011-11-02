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




// Example of new TLS API. Test with:
//
//     $> openssl s_client -connect localhost:12346 \
//        -key test/fixtures/agent.key -cert test/fixtures/agent.crt
//
//     $> openssl s_client -connect localhost:12346
//
var common = require('../common');
var tls = require('tls');
var fs = require('fs');
var join = require('path').join;

var key = fs.readFileSync(join(common.fixturesDir, 'agent.key')).toString();
var cert = fs.readFileSync(join(common.fixturesDir, 'agent.crt')).toString();

s = tls.Server({ key: key,
      cert: cert,
      ca: [],
      requestCert: true,
      rejectUnauthorized: true });

s.listen(common.PORT, function() {
  console.log('TLS server on 127.0.0.1:%d', common.PORT);
});


s.on('authorized', function(c) {
  console.log('authed connection');
  c.end('bye authorized friend.\n');
});

s.on('unauthorized', function(c, e) {
  console.log('unauthed connection: %s', e);
  c.end('bye unauthorized person.\n');
});
