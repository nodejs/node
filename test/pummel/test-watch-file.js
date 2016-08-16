'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var path = require('path');

var f = path.join(common.fixturesDir, 'x.txt');

console.log('watching for changes of ' + f);

var changes = 0;
function watchFile() {
  fs.watchFile(f, function(curr, prev) {
    console.log(f + ' change');
    changes++;
    assert.notDeepStrictEqual(curr.mtime, prev.mtime);
    fs.unwatchFile(f);
    watchFile();
    fs.unwatchFile(f);
  });
}

watchFile();


var fd = fs.openSync(f, 'w+');
fs.writeSync(fd, 'xyz\n');
fs.closeSync(fd);

process.on('exit', function() {
  assert.ok(changes > 0);
});
