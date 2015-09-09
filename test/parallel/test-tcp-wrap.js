'use strict';
// Flags: --expose_internals
var common = require('../common');
var assert = require('assert');

var TCP = require('binding/tcp_wrap').TCP;
var uv = require('binding/uv');

var handle = new TCP();

// Should be able to bind to the common.PORT
var err = handle.bind('0.0.0.0', common.PORT);
assert.equal(err, 0);

// Should not be able to bind to the same port again
err = handle.bind('0.0.0.0', common.PORT);
assert.equal(err, uv.UV_EINVAL);

handle.close();
