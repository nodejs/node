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

var common = require('../common');
var assert = require('assert');

// ASCII conversion in node.js simply masks off the high bits,
// it doesn't do transliteration.
assert.equal(Buffer('hérité').toString('ascii'), 'hC)ritC)');

// 71 characters, 78 bytes. The ’ character is a triple-byte sequence.
var input = 'C’est, graphiquement, la réunion d’un accent aigu ' +
            'et d’un accent grave.';

var expected = 'Cb\u0000\u0019est, graphiquement, la rC)union ' +
               'db\u0000\u0019un accent aigu et db\u0000\u0019un ' +
               'accent grave.';

var buf = Buffer(input);

for (var i = 0; i < expected.length; ++i) {
  assert.equal(buf.slice(i).toString('ascii'), expected.slice(i));

  // Skip remainder of multi-byte sequence.
  if (input.charCodeAt(i) > 65535) ++i;
  if (input.charCodeAt(i) > 127) ++i;
}
