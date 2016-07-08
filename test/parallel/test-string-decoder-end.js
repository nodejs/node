'use strict';
// verify that the string decoder works getting 1 byte at a time,
// the whole buffer at once, and that both match the .toString(enc)
// result of the entire buffer.

require('../common');
var assert = require('assert');
var SD = require('string_decoder').StringDecoder;
var encodings = ['base64', 'hex', 'utf8', 'utf16le', 'ucs2'];

var bufs = [ '☃💩', 'asdf' ].map(function(b) {
  return Buffer.from(b);
});

// also test just arbitrary bytes from 0-15.
for (var i = 1; i <= 16; i++) {
  var bytes = new Array(i).join('.').split('.').map(function(_, j) {
    return j + 0x78;
  });
  bufs.push(Buffer.from(bytes));
}

encodings.forEach(testEncoding);

console.log('ok');

function testEncoding(encoding) {
  bufs.forEach(function(buf) {
    testBuf(encoding, buf);
  });
}

function testBuf(encoding, buf) {
  console.error('# %s', encoding, buf);

  // write one byte at a time.
  var s = new SD(encoding);
  var res1 = '';
  for (var i = 0; i < buf.length; i++) {
    res1 += s.write(buf.slice(i, i + 1));
  }
  res1 += s.end();

  // write the whole buffer at once.
  var res2 = '';
  s = new SD(encoding);
  res2 += s.write(buf);
  res2 += s.end();

  // .toString() on the buffer
  var res3 = buf.toString(encoding);

  console.log('expect=%j', res3);
  console.log('res1=%j', res1);
  console.log('res2=%j', res2);
  assert.equal(res1, res3, 'one byte at a time should match toString');
  assert.equal(res2, res3, 'all bytes at once should match toString');
}
