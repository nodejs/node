'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const {
  hasOpenSSL3,
} = require('../common/crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

let clientClosed = false;
let errorReceived = false;
function canCloseServer() {
  return clientClosed && errorReceived;
}

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`, 'utf-8');
}

const opts = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};

const max_iter = 20;
let iter = 0;

const errorHandler = common.mustCall((err) => {
  // Different OpenSSL versions report different errors for junk data on a
  // TLS connection, depending on which record validation check fires first.
  assert.match(err.code,
               /ERR_SSL_(WRONG_VERSION_NUMBER|PACKET_LENGTH_TOO_LONG|BAD_RECORD_TYPE)/);
  assert.strictEqual(err.library, 'SSL routines');
  if (!hasOpenSSL3 && !process.features.openssl_is_boringssl)
    assert.strictEqual(err.function, 'ssl3_get_record');
  assert.match(err.reason,
               /wrong[\s_]version[\s_]number|packet[\s_]length[\s_]too[\s_]long|bad[\s_]record[\s_]type/i);
  errorReceived = true;
  if (canCloseServer())
    server.close();
});
const server = tls.createServer(opts, common.mustCall(function(s) {
  s.pipe(s);
  s.on('error', errorHandler);
}, 2));

server.listen(0, common.mustCall(function() {
  sendClient();
}));

server.on('tlsClientError', common.mustNotCall());

server.on('error', common.mustNotCall());

function sendClient() {
  const client = tls.connect(server.address().port, {
    rejectUnauthorized: false
  });
  client.on('data', common.mustCall(function() {
    if (iter++ === 2) sendBADTLSRecord();
    if (iter < max_iter) {
      client.write('a');
      return;
    }
    client.end();
  }, max_iter));
  client.write('a', common.mustCall());
  client.on('error', common.mustNotCall());
  client.on('close', common.mustCall(function() {
    clientClosed = true;
    if (canCloseServer())
      server.close();
  }));
}


function sendBADTLSRecord() {
  const BAD_RECORD = Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  const socket = net.connect(server.address().port);
  const client = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    client.write('x');
    client.on('data', (data) => {
      socket.end(BAD_RECORD);
    });
  }));
  client.on('error', common.mustCall((err) => {
    // Different OpenSSL versions send different TLS alerts when the peer
    // receives an invalid record on an established connection.
    assert.match(err.code,
                 /ERR_SSL_(TLSV1_ALERT_PROTOCOL_VERSION|TLSV1_ALERT_RECORD_OVERFLOW|(SSL\/)?TLS_ALERT_UNEXPECTED_MESSAGE)/);
    assert.strictEqual(err.library, 'SSL routines');
    if (!hasOpenSSL3 && !process.features.openssl_is_boringssl)
      assert.strictEqual(err.function, 'ssl3_read_bytes');
    assert.match(err.reason,
                 /tlsv1[\s_]alert[\s_]protocol[\s_]version|tlsv1[\s_]alert[\s_]record[\s_]overflow|(ssl\/)?tls[\s_]alert[\s_]unexpected[\s_]message/i);
  }));
}
