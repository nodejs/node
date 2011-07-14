var common = require('../common');
var assert = require('assert');

var TCP = process.binding('tcp_wrap').TCP;

var handle = new TCP();

// Should be able to bind to the common.PORT
var r = handle.bind("0.0.0.0", common.PORT);
assert.equal(0, r);

// Should not be able to bind to the same port again
var r = handle.bind("0.0.0.0", common.PORT);
assert.equal(-1, r);
console.log(errno);
assert.equal(errno, "EINVAL");

handle.close();
