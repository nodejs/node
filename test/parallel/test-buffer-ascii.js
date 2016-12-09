'use strict';
require('../common');
const assert = require('assert');

// ASCII conversion in node.js simply masks off the high bits,
// it doesn't do transliteration.
assert.strictEqual(Buffer.from('hérité').toString('ascii'), 'hC)ritC)');

// 71 characters, 78 bytes. The ’ character is a triple-byte sequence.
const input = 'C’est, graphiquement, la réunion d’un accent aigu ' +
              'et d’un accent grave.';

const expected = 'Cb\u0000\u0019est, graphiquement, la rC)union ' +
                 'db\u0000\u0019un accent aigu et db\u0000\u0019un ' +
                 'accent grave.';

const buf = Buffer.from(input);

for (var i = 0; i < expected.length; ++i) {
  assert.strictEqual(buf.slice(i).toString('ascii'), expected.slice(i));

  // Skip remainder of multi-byte sequence.
  if (input.charCodeAt(i) > 65535) ++i;
  if (input.charCodeAt(i) > 127) ++i;
}
