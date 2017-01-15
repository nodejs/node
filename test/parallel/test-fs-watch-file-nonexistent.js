'use strict';

var fs = require('fs');

var filename = '/path/to/file/that/does/not/exist';

fs.watchFile(filename, function() {
  throw new Error('callback should not be called for non-existent files');
});

setTimeout(function() {
  fs.unwatchFile(filename);
}, 10);
