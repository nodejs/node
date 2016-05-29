'use strict';
require('../common');
var assert = require('assert');

var TCP = process.binding('tcp_wrap').TCP;
var uv = process.binding('uv');

var handle = new TCP();

// Should be able to bind to the port
var err = handle.bind('0.0.0.0', 0);
assert.equal(err, 0);

// Should not be able to bind to the same port again
var out = {};
handle.getsockname(out);
err = handle.bind('0.0.0.0', out.port);
assert.equal(err, uv.UV_EINVAL);

handle.close();
