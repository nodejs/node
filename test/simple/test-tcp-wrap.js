var common = require('../common');
var assert = require('assert');

var TCP = process.binding('tcp_wrap').TCP;

var handle = new TCP();

// Cannot bind to port 80 because of access rights.
var r = handle.bind("0.0.0.0", 80);
assert.equal(-1, r);
console.log(errno);
assert.equal(errno, "EACCESS");

// Should be able to bind to the common.PORT
var r = handle.bind("0.0.0.0", common.PORT);
assert.equal(0, r);

handle.close();
