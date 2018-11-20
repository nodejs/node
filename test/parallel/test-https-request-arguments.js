'use strict';
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');
const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ca: fixtures.readKey('ca1-cert.pem')
};

// Test providing both a url and options, with the options partially
// replacing address and port portions of the URL provided.
{
  const server = https.createServer(
    options,
    common.mustCall((req, res) => {
      assert.strictEqual(req.url, '/testpath');
      res.end();
      server.close();
    })
  );

  server.listen(
    0,
    common.mustCall(() => {
      https.get(
        'https://example.com/testpath',

        {
          hostname: 'localhost',
          port: server.address().port,
          rejectUnauthorized: false
        },

        common.mustCall((res) => {
          res.resume();
        })
      );
    })
  );
}
