'use strict';
const common = require('../common');
const assert = require('assert');

const fs = require('fs');
const path = require('path');

const f = path.join(common.fixturesDir, 'x.txt');

let changes = 0;
function watchFile() {
  fs.watchFile(f, (curr, prev) => {
    changes++;
    assert.notDeepStrictEqual(curr.mtime, prev.mtime);
    fs.unwatchFile(f);
    watchFile();
    fs.unwatchFile(f);
  });
}

watchFile();


const fd = fs.openSync(f, 'w+');
fs.writeSync(fd, 'xyz\n');
fs.closeSync(fd);

process.on('exit', function() {
  assert.ok(changes > 0);
});
