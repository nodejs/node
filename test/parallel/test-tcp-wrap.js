'use strict';
require('../common');
const assert = require('assert');

const TCP = process.binding('tcp_wrap').TCP;
const uv = process.binding('uv');

const handle = new TCP();

// Should be able to bind to the port
let err = handle.bind('0.0.0.0', 0);
assert.equal(err, 0);

// Should not be able to bind to the same port again
const out = {};
handle.getsockname(out);
err = handle.bind('0.0.0.0', out.port);
assert.equal(err, uv.UV_EINVAL);

handle.close();
