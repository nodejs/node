'use strict';

require('../common');
const assert = require('assert');

const expectedAPI = {
  _http_agent: ['Agent', 'globalAgent'],
  _http_client: ['ClientRequest'],
  _http_common: [
    'CRLF',
    'HTTPParser',
    '_checkInvalidHeaderChar',
    '_checkIsHttpToken',
    'chunkExpression',
    'continueExpression',
    'debug',
    'freeParser',
    'isLenient',
    'kIncomingMessage',
    'methods',
    'parsers',
    'prepareError',
  ],
  _http_incoming: ['IncomingMessage', 'readStart', 'readStop'],
  _http_outgoing: ['OutgoingMessage'],
  _http_server: [
    'STATUS_CODES',
    'Server',
    'ServerResponse',
    '_connectionListener',
    'kServerResponse',
  ],
  _stream_duplex: [],
  _stream_passthrough: [],
  _stream_readable: ['ReadableState', '_fromList', 'from'],
  _stream_transform: [],
  _stream_wrap: [],
  _stream_writable: ['WritableState'],
  _tls_common: [
    'SecureContext',
    'createSecureContext',
    'translatePeerCertificate',
  ],
  _tls_wrap: ['Server', 'TLSSocket', 'connect', 'createServer'],
};

for (const [moduleName, exports] of Object.entries(expectedAPI)) {
  if (exports.length) {
    exports.forEach((exportName) => assert(exportName in require(moduleName)));
  } else {
    // Only a default export
    assert.ok(require(moduleName));
  }
}
