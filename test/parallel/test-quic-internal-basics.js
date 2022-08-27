// Flags: --expose-internals --no-warnings
'use strict';

require('../common');

const { internalBinding } = require('internal/test/binding');
const {
  strictEqual,
} = require('assert');

const test = require('node:test');

test('The internal binding is there', () => {

  const quic = internalBinding('quic');

  test('It has the stuff we expect...', () => {
    strictEqual(typeof quic.setCallbacks, 'function');
    strictEqual(typeof quic.createEndpoint, 'function');

    strictEqual(typeof quic.EndpointOptions, 'function');
    strictEqual(typeof quic.SessionOptions, 'function');
    strictEqual(typeof quic.ArrayBufferViewSource, 'function');
    strictEqual(typeof quic.StreamSource, 'function');
    strictEqual(typeof quic.StreamBaseSource, 'function');
    strictEqual(typeof quic.BlobSource, 'function');

    strictEqual(quic.QUIC_CC_ALGO_CUBIC, 1);
    strictEqual(quic.QUIC_CC_ALGO_RENO, 0);
    strictEqual(quic.QUIC_MAX_CIDLEN, 20);
    strictEqual(quic.QUIC_ERR_NO_ERROR, 0);
    strictEqual(quic.QUIC_ERR_INTERNAL_ERROR, 1);
    strictEqual(quic.QUIC_ERR_CONNECTION_REFUSED, 2);
    strictEqual(quic.QUIC_ERR_FLOW_CONTROL_ERROR, 3);
    strictEqual(quic.QUIC_ERR_STREAM_LIMIT_ERROR, 4);
    strictEqual(quic.QUIC_ERR_STREAM_STATE_ERROR, 5);
    strictEqual(quic.QUIC_ERR_FINAL_SIZE_ERROR, 6);
    strictEqual(quic.QUIC_ERR_FRAME_ENCODING_ERROR, 7);
    strictEqual(quic.QUIC_ERR_TRANSPORT_PARAMETER_ERROR, 8);
    strictEqual(quic.QUIC_ERR_CONNECTION_ID_LIMIT_ERROR, 9);
    strictEqual(quic.QUIC_ERR_PROTOCOL_VIOLATION, 10);
    strictEqual(quic.QUIC_ERR_INVALID_TOKEN, 11);
    strictEqual(quic.QUIC_ERR_APPLICATION_ERROR, 12);
    strictEqual(quic.QUIC_ERR_CRYPTO_BUFFER_EXCEEDED, 13);
    strictEqual(quic.QUIC_ERR_KEY_UPDATE_ERROR, 14);
    strictEqual(quic.QUIC_ERR_AEAD_LIMIT_REACHED, 15);
    strictEqual(quic.QUIC_ERR_NO_VIABLE_PATH, 16);
    strictEqual(quic.QUIC_ERR_CRYPTO_ERROR, 256);
    strictEqual(quic.QUIC_ERR_VERSION_NEGOTIATION_ERROR_DRAFT, 21496);
    strictEqual(quic.QUIC_PREFERRED_ADDRESS_IGNORE, 0);
    strictEqual(quic.QUIC_PREFERRED_ADDRESS_USE, 1);

    strictEqual(quic.QUIC_DEFAULT_CIPHERS, 'TLS_AES_128_GCM_SHA256:' +
                                           'TLS_AES_256_GCM_SHA384:' +
                                           'TLS_CHACHA20_POLY1305_' +
                                           'SHA256:TLS_AES_128_CCM_SHA256');
    strictEqual(quic.QUIC_DEFAULT_GROUPS, 'X25519:P-256:P-384:P-521');
  });
});
