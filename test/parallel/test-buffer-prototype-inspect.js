'use strict';
require('../common');

// lib/buffer.js defines Buffer.prototype.inspect() to override how buffers are
// presented by util.inspect().

const assert = require('assert');
const util = require('util');

{
  const buf = Buffer.from('fhqwhgads');
  assert.strictEqual(util.inspect(buf), '<Buffer 66 68 71 77 68 67 61 64 73>');
}

{
  const buf = Buffer.from('');
  assert.strictEqual(util.inspect(buf), '<Buffer >');
}

{
  const buf = Buffer.from('x'.repeat(51));
  assert.ok(/^<Buffer (78 ){50}\.\.\. >$/.test(util.inspect(buf)));
}
