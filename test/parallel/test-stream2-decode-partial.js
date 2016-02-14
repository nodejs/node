'use strict';
require('../common');
const Readable = require('_stream_readable');
const assert = require('assert');

var buf = '';
const euro = new Buffer([0xE2, 0x82, 0xAC]);
const cent = new Buffer([0xC2, 0xA2]);
const source = Buffer.concat([euro, cent]);

const readable = Readable({ encoding: 'utf8' });
readable.push(source.slice(0, 2));
readable.push(source.slice(2, 4));
readable.push(source.slice(4, 6));
readable.push(null);

readable.on('data', function(data) {
  buf += data;
});

process.on('exit', function() {
  assert.strictEqual(buf, '€¢');
});
