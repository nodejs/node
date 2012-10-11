// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// verify that the string decoder works getting 1 byte at a time,
// the whole buffer at once, and that both match the .toString(enc)
// result of the entire buffer.

var assert = require('assert');
var SD = require('string_decoder').StringDecoder;
var encodings = ['base64', 'hex', 'utf8', 'utf16le', 'ucs2'];

var bufs = [ '☃💩', 'asdf' ].map(function(b) {
  return new Buffer(b);
});

// also test just arbitrary bytes from 0-15.
for (var i = 1; i <= 16; i++) {
  var bytes = new Array(i).join('.').split('.').map(function(_, j) {
    return j + 0x78;
  });
  bufs.push(new Buffer(bytes));
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
  var s = new SD(encoding);
  res2 += s.write(buf);
  res2 += s.end();

  // .toString() on the buffer
  var res3 = buf.toString(encoding);

  console.log('expect=%j', res3);
  assert.equal(res1, res3, 'one byte at a time should match toString');
  assert.equal(res2, res3, 'all bytes at once should match toString');
}
