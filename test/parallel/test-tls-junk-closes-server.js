'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

var server = tls.createServer(options, function(s) {
  s.write('welcome!\n');
  s.pipe(s);
});

server.listen(common.PORT, function() {
  var c = net.createConnection(common.PORT);

  c.on('connect', function() {
    c.write('blah\nblah\nblah\n');
  });

  c.on('end', function() {
    server.close();
  });

});
