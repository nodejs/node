'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// Test directly created TLS sockets and options.

const assert = require('assert');
const {
  connect, keys, tls
} = require(fixtures.path('tls-connect'));

test(undefined, (err) => {
  assert.strictEqual(err.message, 'unable to verify the first certificate');
});

test({}, (err) => {
  assert.strictEqual(err.message, 'unable to verify the first certificate');
});

test({secureContext: tls.createSecureContext({ca: keys.agent1.ca})}, (err) => {
  assert.ifError(err);
});

test({ca: keys.agent1.ca}, (err) => {
  assert.ifError(err);
});

// Secure context options, like ca, are ignored if a sec ctx is explicitly
// provided.
test({secureContext: tls.createSecureContext(), ca: keys.agent1.ca}, (err) => {
  assert.strictEqual(err.message, 'unable to verify the first certificate');
});

function test(client, callback) {
  callback = common.mustCall(callback);
  connect({
    server: {
      key: keys.agent1.key,
      cert: keys.agent1.cert,
    },
  }, function(err, pair, cleanup) {
    assert.strictEqual(err.message, 'unable to verify the first certificate');
    let recv = '';
    pair.server.server.once('secureConnection', common.mustCall((conn) => {
      conn.on('data', (data) => recv += data);
      conn.on('end', common.mustCall(() => {
        // Server sees nothing wrong with connection, even though the client's
        // authentication of the server cert failed.
        assert.strictEqual(recv, 'hello');
        cleanup();
      }));
    }));

    // Client doesn't support the 'secureConnect' event, and doesn't error if
    // authentication failed. Caller must explicitly check for failure.
    (new tls.TLSSocket(null, client)).connect(pair.server.server.address().port)
      .on('connect', common.mustCall(function() {
        this.end('hello');
      }))
      .on('secure', common.mustCall(function() {
        callback(this.ssl.verifyError());
      }));
  });
}
