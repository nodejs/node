require('../common');
var path = require('path')
  , Buffer = require('buffer').Buffer
  , fs = require('fs')
  , fn = path.join(fixturesDir, 'write.txt');

var fd = fs.openSync(fn, 'w');
fs.writeSync(fd, 'foo');
fs.writeSync(fd, new Buffer('bar'), 0, 3);
fs.closeSync(fd);

assert.equal(fs.readFileSync(fn), 'foobar');