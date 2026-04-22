'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

// Test that mutating req.path after construction to include
// invalid characters (e.g. CRLF) throws ERR_UNESCAPED_CHARACTERS.
// Regression test for a TOCTOU vulnerability where path was only
// validated at construction time but could be mutated before
// _implicitHeader() flushed it to the socket.

// Use a createConnection that returns nothing to avoid actual connection.
const req = new http.ClientRequest({
  host: '127.0.0.1',
  port: 1,
  path: '/valid',
  method: 'GET',
  createConnection: () => {},
});

// Attempting to set path with CRLF must throw
assert.throws(
  () => { req.path = '/evil\r\nX-Injected: true\r\n\r\n'; },
  {
    code: 'ERR_UNESCAPED_CHARACTERS',
    name: 'TypeError',
    message: 'Request path contains unescaped characters',
  }
);

// Path must be unchanged after failed mutation
assert.strictEqual(req.path, '/valid');

// Attempting to set path with lone CR must throw
assert.throws(
  () => { req.path = '/evil\rpath'; },
  {
    code: 'ERR_UNESCAPED_CHARACTERS',
    name: 'TypeError',
  }
);

// Attempting to set path with lone LF must throw
assert.throws(
  () => { req.path = '/evil\npath'; },
  {
    code: 'ERR_UNESCAPED_CHARACTERS',
    name: 'TypeError',
  }
);

// Attempting to set path with null byte must throw
assert.throws(
  () => { req.path = '/evil\0path'; },
  {
    code: 'ERR_UNESCAPED_CHARACTERS',
    name: 'TypeError',
  }
);

// Valid path mutation should succeed
req.path = '/also-valid';
assert.strictEqual(req.path, '/also-valid');

req.path = '/path?query=1&other=2';
assert.strictEqual(req.path, '/path?query=1&other=2');

req.destroy();
