'use strict';

// Technically, file URLs are not permitted to have a port. There is
// currently an ambiguity in the URL spec. In the current spec
// having a port in a file URL is undefined behavior. In the current
// implementation, the port is ignored and handled as if it were part
// of the host name. This will be changing once the ambiguity is
// resolved in the spec. The spec change may involve either ignoring the
// port if specified or throwing with an Invalid URL error if the port
// is specified. For now, this test verifies the currently implemented
// behavior.

require('../common');
const URL = require('url').URL;
const assert = require('assert');

const u = new URL('file://example.net:80/foo');

assert.strictEqual(u.hostname, 'example.net:80');
assert.strictEqual(u.port, '');

u.host = 'example.com:81';

assert.strictEqual(u.hostname, 'example.com:81');
assert.strictEqual(u.port, '');

u.hostname = 'example.org:81';

assert.strictEqual(u.hostname, 'example.org');
assert.strictEqual(u.port, '');
