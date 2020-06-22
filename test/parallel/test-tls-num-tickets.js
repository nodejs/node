'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
});

const expectedNumTickets = 1;
// 2 is the deafult value set by OpenSSL.
assert.strictEqual(server.getNumTickets(), 2);
// Change the number of sent session tickets.
server.setNumTickets(expectedNumTickets);
// Ensure that it has indeed been changed.
assert.strictEqual(server.getNumTickets(), expectedNumTickets);
// Ensure that an error is thrown when attempting to set the number of tickets
// to a negative number.
assert.throws(
  () => server.setNumTickets(-1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Number of tickets must be an unsigned 32-bit integer'
  }
);

// Ensure that an error is thrown when attempting to set the number of tickets
// to Mongolian throat singing.
assert.throws(
  () => server.setNumTickets('HURRMMMM'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Number of tickets must be an unsigned 32-bit integer'
  }
);
