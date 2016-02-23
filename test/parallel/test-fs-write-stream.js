'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

var file = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

(function() {
  const stream = fs.WriteStream(file);
  const _fs_close = fs.close;

  fs.close = function(fd) {
    assert.ok(fd, 'fs.close must not be called without an undefined fd.');
    fs.close = _fs_close;
  };
  stream.destroy();
})();

(function() {
  var stream = fs.createWriteStream(file);

  stream.on('drain', function() {
    assert.fail(null, null, '\'drain\' event must not be emitted before ' +
                'stream.write() has been called at least once.');
  });
  stream.destroy();
})();

