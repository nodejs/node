'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const tls = require('tls');
const fs = require('fs');

const clientConfigs = [
  { secureProtocol: 'TLSv1_method', version: 'TLSv1' },
  { secureProtocol: 'TLSv1_1_method', version: 'TLSv1.1' },
  { secureProtocol: 'TLSv1_2_method', version: 'TLSv1.2' }
];

const serverConfig = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

const server = tls.createServer(serverConfig, common.mustCall(function() {

}, clientConfigs.length)).listen(0, common.localhostIPv4, function() {
  let connected = 0;
  clientConfigs.forEach(function(v) {
    tls.connect({
      host: common.localhostIPv4,
      port: server.address().port,
      rejectUnauthorized: false,
      secureProtocol: v.secureProtocol
    }, common.mustCall(function() {
      assert.strictEqual(this.getProtocol(), v.version);
      this.on('end', common.mustCall(function() {
        assert.strictEqual(this.getProtocol(), null);
      })).end();
      if (++connected === clientConfigs.length)
        server.close();
    }));
  });
});
