'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');

const tls = require('tls');

const fs = require('fs');

const dir = common.fixturesDir;
const options = { key: fs.readFileSync(dir + '/test_key.pem'),
                  cert: fs.readFileSync(dir + '/test_cert.pem'),
                  ca: [ fs.readFileSync(dir + '/test_ca.pem') ] };

const writes = [
  'some server data',
  'and a separate packet',
  'and one more',
];
let receivedWrites = 0;

const server = tls.createServer(options, function(c) {
  writes.forEach(function(str) {
    c.write(str);
  });
}).listen(0, common.mustCall(function() {
  const connectOpts = { rejectUnauthorized: false };
  const c = tls.connect(this.address().port, connectOpts, function() {
    c.write('some client data');
    c.on('readable', function() {
      let data = c.read();
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
}));


process.on('exit', function() {
  assert.strictEqual(receivedWrites, writes.length);
});
