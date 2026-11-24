'use strict';
require('../common');
const assert = require('node:assert');
const { styleText } = require('node:util');

// Closing a nested style must restore the outer formats in their original order.
const options = { validateStream: false };
for (const [first, second, nested] of [
  ['red', 'green', 'blue'],
  ['red', '#0f0', 'blue'],
  ['#f00', 'green', 'blue'],
  ['bgRed', 'bgGreen', 'bgBlue'],
  ['bold', 'dim', 'bold'],
]) {
  const text = 'before' + styleText(nested, 'inside', options) + 'after';
  assert.strictEqual(
    styleText([first, second], text, options),
    styleText(first, styleText(second, text, options), options),
    `Restore ${first}, ${second} after ${nested}`,
  );
}
