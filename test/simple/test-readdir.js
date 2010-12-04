var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var got_error = false,
    readdirDir = path.join(common.fixturesDir, 'readdir');

var files = ['are',
             'dir',
             'empty',
             'files',
             'for',
             'just',
             'testing.js',
             'these'];


console.log('readdirSync ' + readdirDir);
var f = fs.readdirSync(readdirDir);
console.dir(f);
assert.deepEqual(files, f.sort());


console.log('readdir ' + readdirDir);
fs.readdir(readdirDir, function(err, f) {
  if (err) {
    console.log('error');
    got_error = true;
  } else {
    console.dir(f);
    assert.deepEqual(files, f.sort());
  }
});

process.addListener('exit', function() {
  assert.equal(false, got_error);
  console.log('exit');
});
