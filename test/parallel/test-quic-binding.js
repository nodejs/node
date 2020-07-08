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

assert.strictEqual(quic.constants.NGTCP2_NO_ERROR, 0);
assert.strictEqual(quic.constants.NGTCP2_INTERNAL_ERROR, 1);
assert.strictEqual(quic.constants.NGTCP2_CONNECTION_REFUSED, 2);
assert.strictEqual(quic.constants.NGTCP2_FLOW_CONTROL_ERROR, 3);
assert.strictEqual(quic.constants.NGTCP2_STREAM_LIMIT_ERROR, 4);
assert.strictEqual(quic.constants.NGTCP2_STREAM_STATE_ERROR, 5);
assert.strictEqual(quic.constants.NGTCP2_FINAL_SIZE_ERROR, 6);
assert.strictEqual(quic.constants.NGTCP2_FRAME_ENCODING_ERROR, 7);
assert.strictEqual(quic.constants.NGTCP2_TRANSPORT_PARAMETER_ERROR, 8);
assert.strictEqual(quic.constants.NGTCP2_CONNECTION_ID_LIMIT_ERROR, 9);
assert.strictEqual(quic.constants.NGTCP2_PROTOCOL_VIOLATION, 0xa);
assert.strictEqual(quic.constants.NGTCP2_INVALID_TOKEN, 0xb);
assert.strictEqual(quic.constants.NGTCP2_APPLICATION_ERROR, 0xc);
assert.strictEqual(quic.constants.NGTCP2_CRYPTO_BUFFER_EXCEEDED, 0xd);
assert.strictEqual(quic.constants.NGTCP2_KEY_UPDATE_ERROR, 0xe);
assert.strictEqual(quic.constants.NGTCP2_CRYPTO_ERROR, 0x100);

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
