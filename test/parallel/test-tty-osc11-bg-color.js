'use strict';
const common = require('../common');

if (!common.isMainThread)
  common.skip('process.stdout is not a tty in Workers');

const assert = require('assert');
const tty = require('tty');

// getBackgroundColor() must always return a Promise, and that Promise
// must settle (never hang forever) even when the terminal doesn't
// respond. Use a short timeout override so this doesn't slow down CI
// on machines without a responsive terminal attached.
const stream = new tty.WriteStream(1);

assert.strictEqual(typeof stream.getBackgroundColor, 'function');

const result = stream.getBackgroundColor({ timeout: 50 });
assert.ok(result instanceof Promise);

result.then(common.mustCall((color) => {
  if (color !== undefined) {
    assert.strictEqual(typeof color.r, 'number');
    assert.strictEqual(typeof color.g, 'number');
    assert.strictEqual(typeof color.b, 'number');
    for (const channel of [color.r, color.g, color.b]) {
      assert.ok(channel >= 0 && channel <= 255);
    }
  }
}));