import '../common/index.mjs';

import assert from 'assert';

[
  ['CONNECTING', 0],
  ['OPEN', 1],
  ['CLOSED', 2],
].forEach((config) => {
  {
    const [constant, value] = config;

    // EventSource exposes the constant.
    assert.strictEqual(Object.hasOwn(EventSource, constant), true);

    // The value is properly set.
    assert.strictEqual(EventSource[constant], value);

    // The constant is enumerable.
    assert.strictEqual(Object.prototype.propertyIsEnumerable.call(EventSource, constant), true);

    // The constant is not writable.
    try {
      EventSource[constant] = 666;
    } catch (e) {
      assert.strictEqual(e instanceof TypeError, true);
    }
    // The constant is not configurable.
    try {
      delete EventSource[constant];
    } catch (e) {
      assert.strictEqual(e instanceof TypeError, true);
    }
    assert.strictEqual(EventSource[constant], value);
  }
});
