'use strict';

require('../common');
const assert = require('assert');

{
  const buf = Buffer.from('123');
  const num = buf.toInt(10);

  assert.strictEqual(num, 123);
}

{
  const buf = Buffer.from('010');
  const num = buf.toInt();

  assert.strictEqual(num, 10);
}

{
  const buf = Buffer.from('123');
  const num = buf.toInt(16);

  assert.strictEqual(num, 0x123);
}
