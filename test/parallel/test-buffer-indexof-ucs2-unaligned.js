'use strict';

// A Buffer view can start at an odd byte offset, so its data is not
// necessarily aligned for two byte reads. The UCS2 search must still work
// rather than hang or report the wrong offset.
// Refs: https://github.com/nodejs/node/issues/65959

require('../common');
const assert = require('assert');

const text = 'abc';

for (const prefixLength of [0, 1, 2, 3]) {
  const data = Buffer.from(text, 'utf16le');
  const backing = Buffer.alloc(data.length + prefixLength);
  data.copy(backing, prefixLength);
  const view = backing.subarray(prefixLength);
  const label = `prefixLength=${prefixLength}`;

  // A view at an odd offset holds the same bytes as an aligned one, so every
  // lookup below has the same answer either way.
  for (let i = 0; i < text.length; i++) {
    const needle = text[i];
    const expected = i * 2;

    assert.strictEqual(
      view.indexOf(needle, 0, 'utf16le'),
      expected,
      `indexOf(${needle}) ${label}`,
    );
    assert.strictEqual(
      view.indexOf(Buffer.from(needle, 'utf16le'), 0, 'utf16le'),
      expected,
      `indexOf(Buffer ${needle}) ${label}`,
    );
    assert.strictEqual(
      view.lastIndexOf(needle, undefined, 'utf16le'),
      expected,
      `lastIndexOf(${needle}) ${label}`,
    );
    assert.ok(
      view.includes(needle, 0, 'utf16le'),
      `includes(${needle}) ${label}`,
    );
  }

  // A needle that is not present is still reported as missing.
  assert.strictEqual(view.indexOf('z', 0, 'utf16le'), -1, `indexOf(z) ${label}`);
  assert.strictEqual(
    view.lastIndexOf('z', undefined, 'utf16le'),
    -1,
    `lastIndexOf(z) ${label}`,
  );
  assert.ok(!view.includes('z', 0, 'utf16le'), `includes(z) ${label}`);
}

// A multi character needle, so the search does more than one comparison.
{
  const data = Buffer.from('hello world', 'utf16le');
  const backing = Buffer.alloc(data.length + 1);
  data.copy(backing, 1);
  const view = backing.subarray(1);

  assert.strictEqual(view.indexOf('world', 0, 'utf16le'), 12);
  assert.strictEqual(view.lastIndexOf('o', undefined, 'utf16le'), 14);
  assert.strictEqual(view.indexOf('worlds', 0, 'utf16le'), -1);
}
