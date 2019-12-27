// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const quic = internalBinding('quic');
assert(quic);

// Version numbers used to identify IETF drafts are created by adding the draft
// number to 0xff0000, in this case 18 (24).
assert.strictEqual(quic.constants.NGTCP2_PROTO_VER.toString(16), 'ff000018');
assert.strictEqual(quic.constants.NGTCP2_ALPN_H3, '\u0005h3-24');

// The following just tests for the presence of things we absolutely need.
// They don't test the functionality of those things.

assert(quic.sessionConfig instanceof Float64Array);
assert(quic.http3Config instanceof Float64Array);

assert(quic.QuicSocket);
assert(quic.QuicEndpoint);
assert(quic.QuicStream);

assert.strictEqual(typeof quic.createClientSession, 'function');
assert.strictEqual(typeof quic.openBidirectionalStream, 'function');
assert.strictEqual(typeof quic.openUnidirectionalStream, 'function');
assert.strictEqual(typeof quic.setCallbacks, 'function');
assert.strictEqual(typeof quic.initSecureContext, 'function');
assert.strictEqual(typeof quic.initSecureContextClient, 'function');
