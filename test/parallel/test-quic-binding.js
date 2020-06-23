// Flags: --expose-internals --no-warnings
'use strict';

// Tests the availability and correctness of internalBinding(quic)

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { internalBinding } = require('internal/test/binding');

const quic = internalBinding('quic');
assert(quic);

assert(quic.constants);

// Version numbers used to identify IETF drafts are created by
// adding the draft number to 0xff0000, in this case 19 (25).
assert.strictEqual(quic.constants.NGTCP2_PROTO_VER.toString(16), 'ff00001d');
assert.strictEqual(quic.constants.NGHTTP3_ALPN_H3, '\u0005h3-29');

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
assert.strictEqual(typeof quic.silentCloseSession, 'function');
