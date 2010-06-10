require('../common');
path = require('path'),
Buffer = require('buffer').Buffer
fs = require('fs')
fn = path.join(fixturesDir, 'write.txt');


foo = 'foo'
var fd = fs.openSync(fn, 'w');
fs.writeSync(fd, foo);

bar = 'bár'
fs.writeSync(fd, new Buffer(bar), 0, Buffer.byteLength(bar));
fs.closeSync(fd);

assert.equal(fs.readFileSync(fn), 'foobár');
