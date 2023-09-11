'use strict';

require('../common');
const assert = require('assert');

{
  assert.strictEqual(JSON.stringify(Buffer.alloc(0)),
                     '{"type":"Buffer","data":[]}');
  assert.strictEqual(JSON.stringify(Buffer.from([1, 2, 3, 4])),
                     '{"type":"Buffer","data":[1,2,3,4]}');
}

// issue GH-7849
{
  const buf = Buffer.from('test');
  const json = JSON.stringify(buf);
  const obj = JSON.parse(json);
  const copy = Buffer.from(obj);

  assert.deepStrictEqual(buf, copy);
}

// GH-5110
{
  const buffer = Buffer.from('test');
  const string = JSON.stringify(buffer);

  assert.strictEqual(string, '{"type":"Buffer","data":[116,101,115,116]}');

  function receiver(key, value) {
    return value && value.type === 'Buffer' ? Buffer.from(value.data) : value;
  }

  assert.deepStrictEqual(buffer, JSON.parse(string, receiver));
}
