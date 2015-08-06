'use strict';
const common = require('../common');
const R = require('_stream_readable');
const W = require('_stream_writable');
const assert = require('assert');

var src = new R({encoding: 'base64'});
var dst = new W();
var hasRead = false;
var accum = [];
var timeout;

src._read = function(n) {
  if(!hasRead) {
    hasRead = true;
    process.nextTick(function() {
      src.push(new Buffer('1'));
      src.push(null);
    });
  };
};

dst._write = function(chunk, enc, cb) {
  accum.push(chunk);
  cb();
};

src.on('end', function() {
  assert.equal(Buffer.concat(accum) + '', 'MQ==');
  clearTimeout(timeout);
});

src.pipe(dst);

timeout = setTimeout(function() {
  assert.fail('timed out waiting for _write');
}, 100);
