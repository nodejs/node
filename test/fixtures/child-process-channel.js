var assert = require('assert');

console.log("NODE_CHANNEL_FD", process.env.NODE_CHANNEL_FD);
assert.ok(process.env.NODE_CHANNEL_FD);

var fd = parseInt(process.env.NODE_CHANNEL_FD);
assert.ok(fd >= 0);

process.exit(0);
