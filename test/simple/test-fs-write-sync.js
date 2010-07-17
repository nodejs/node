common = require("../common");
assert = common.assert
path = require('path'),
Buffer = require('buffer').Buffer
fs = require('fs')
fn = path.join(common.fixturesDir, 'write.txt');


foo = 'foo'
var fd = fs.openSync(fn, 'w');
fs.writeSync(fd, foo);

bar = 'bár'
written = fs.writeSync(fd, new Buffer(bar), 0, Buffer.byteLength(bar));
assert.ok(written > 3);
fs.closeSync(fd);

assert.equal(fs.readFileSync(fn), 'foobár');
