'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var PORT = common.PORT;
var dir = common.fixturesDir;
var options = { key: fs.readFileSync(dir + '/test_key.pem'),
                cert: fs.readFileSync(dir + '/test_cert.pem'),
                ca: [ fs.readFileSync(dir + '/test_ca.pem') ] };

var writes = [
  'some server data',
  'and a separate packet',
  'and one more',
];
var receivedWrites = 0;

var server = tls.createServer(options, function(c) {
  writes.forEach(function(str) {
    c.write(str);
  });
}).listen(PORT, function() {
  var c = tls.connect(PORT, { rejectUnauthorized: false }, function() {
    c.write('some client data');
    c.on('readable', function() {
      var data = c.read();
      if (data === null)
        return;

      data = data.toString();
      while (data.length !== 0) {
        assert.strictEqual(data.indexOf(writes[receivedWrites]), 0);
        data = data.slice(writes[receivedWrites].length);

        if (++receivedWrites === writes.length) {
          c.end();
          server.close();
        }
      }
    });
  });
});

process.on('exit', function() {
  assert.equal(receivedWrites, writes.length);
});
