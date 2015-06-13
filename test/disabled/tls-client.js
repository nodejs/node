'use strict';
var common = require('../common');
var tls = require('tls');
var fs = require('fs');


// most servers don't require certificates

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem')
};


var s = tls.connect(443, 'joyent.com', options, function() {
  if (!s.authorized) {
    console.error('CONNECTED: ' + s.authorizationError);
    s.destroy();
    return;
  }
  s.pipe(process.stdout);
  process.openStdin().pipe(s);
});

