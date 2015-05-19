'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var N = 100;
var j = 0;

for (var i = 0; i < N; i++) {
  // these files don't exist
  fs.stat('does-not-exist-' + i, function(err) {
    if (err) {
      j++; // only makes it to about 17
      console.log('finish ' + j);
    } else {
      throw new Error('this shouldn\'t be called');
    }
  });
}

process.on('exit', function() {
  assert.equal(N, j);
});
