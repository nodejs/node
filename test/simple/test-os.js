var common = require('../common');
var assert = require('assert');
var os = require('os');

assert.ok(os.hostname().length > 0);
assert.ok(os.loadavg().length > 0);
assert.ok(os.uptime() > 0);
assert.ok(os.freemem() > 0);
assert.ok(os.totalmem() > 0);
assert.ok(os.cpus().length > 0);
assert.ok(os.type().length > 0);
assert.ok(os.release().length > 0);