'use strict';
// test convenience methods with and without options supplied

require('../common');
const assert = require('assert');
const zlib = require('zlib');

var hadRun = 0;

const expect = 'blahblahblahblahblahblah';
const opts = {
  level: 9,
  chunkSize: 1024,
};

[
  ['gzip', 'gunzip'],
  ['gzip', 'unzip'],
  ['deflate', 'inflate'],
  ['deflateRaw', 'inflateRaw'],
].forEach(function(method) {

  zlib[method[0]](expect, opts, function(err, result) {
    zlib[method[1]](result, opts, function(err, result) {
      assert.equal(result, expect,
                   'Should get original string after ' +
                   method[0] + '/' + method[1] + ' with options.');
      hadRun++;
    });
  });

  zlib[method[0]](expect, function(err, result) {
    zlib[method[1]](result, function(err, result) {
      assert.equal(result, expect,
                   'Should get original string after ' +
                   method[0] + '/' + method[1] + ' without options.');
      hadRun++;
    });
  });

  var result = zlib[method[0] + 'Sync'](expect, opts);
  result = zlib[method[1] + 'Sync'](result, opts);
  assert.equal(result, expect,
               'Should get original string after ' +
               method[0] + '/' + method[1] + ' with options.');
  hadRun++;

  result = zlib[method[0] + 'Sync'](expect);
  result = zlib[method[1] + 'Sync'](result);
  assert.equal(result, expect,
               'Should get original string after ' +
               method[0] + '/' + method[1] + ' without options.');
  hadRun++;

});

process.on('exit', function() {
  assert.equal(hadRun, 16, 'expect 16 compressions');
});
