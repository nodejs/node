'use strict';
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
