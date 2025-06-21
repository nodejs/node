// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const {
  ArrayOfApply,
  ArrayPrototypePushApply,
  ArrayPrototypeUnshiftApply,
  MathMaxApply,
  MathMinApply,
  StringPrototypeConcatApply,
  TypedArrayOfApply,
} = require('internal/test/binding').primordials;

{
  const arr1 = [1, 2, 3];
  const arr2 = ArrayOfApply(arr1);

  assert.deepStrictEqual(arr2, arr1);
  assert.notStrictEqual(arr2, arr1);
}

{
  const array = [1, 2, 3];
  const i32Array = TypedArrayOfApply(Int32Array, array);

  assert(i32Array instanceof Int32Array);
  assert.strictEqual(i32Array.length, array.length);
  for (let i = 0, { length } = array; i < length; i++) {
    assert.strictEqual(i32Array[i], array[i], `i32Array[${i}] === array[${i}]`);
  }
}

{
  const arr1 = [1, 2, 3];
  const arr2 = [4, 5, 6];

  const expected = [...arr1, ...arr2];

  assert.strictEqual(ArrayPrototypePushApply(arr1, arr2), expected.length);
  assert.deepStrictEqual(arr1, expected);
}

{
  const arr1 = [1, 2, 3];
  const arr2 = [4, 5, 6];

  const expected = [...arr2, ...arr1];

  assert.strictEqual(ArrayPrototypeUnshiftApply(arr1, arr2), expected.length);
  assert.deepStrictEqual(arr1, expected);
}

{
  const array = [1, 2, 3];
  assert.strictEqual(MathMaxApply(array), 3);
  assert.strictEqual(MathMinApply(array), 1);
}

{
  let hint;
  const obj = { [Symbol.toPrimitive](h) {
    hint = h;
    return '[object Object]';
  } };

  const args = ['foo ', obj, ' bar'];
  const result = StringPrototypeConcatApply('', args);

  assert.strictEqual(hint, 'string');
  assert.strictEqual(result, 'foo [object Object] bar');
}
